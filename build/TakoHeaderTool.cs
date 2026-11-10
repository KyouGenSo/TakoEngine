// TakoReflection.props から RoslynCodeTaskFactory で読み込む MSBuild タスク。
// ヘッダーの TCLASS / TSTRUCT / TENUM / TPROPERTY を読み、StaticType・GetType・Reflect の定義と型の登録関数を 1 つの .cpp に書き出す。
// MSBuild のプロセスはビルドをまたいで再利用されるので、このファイルを変えたら VS を再起動しないと反映されないことがある。
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using Microsoft.Build.Framework;
using Microsoft.Build.Utilities;

public class TakoHeaderTool : Task
{
    [Required] public ITaskItem[] Headers { get; set; }
    [Required] public string OutputFile { get; set; }
    [Required] public string ModuleName { get; set; }

    private enum TokenKind { Identifier, Number, Literal, Punct }

    private struct Token
    {
        public TokenKind Kind;
        public string Text;
        public int Start;
        public int End;
    }

    private class EnumDecl
    {
        public string Name;
        public ClassDecl Owner;
        public List<string> Enumerators = new List<string>();
    }

    private class PropertyDecl
    {
        public string Member;
        public string TypeText;
        public List<KeyValuePair<string, string>> Specifiers = new List<KeyValuePair<string, string>>();
        public int Line;
    }

    private class ClassDecl
    {
        public string Name;
        public string Base;
        public bool IsStruct;
        public ClassDecl Owner;  // TSTRUCT を入れ子で書いたときの外側
        public List<string> Namespaces = new List<string>();
        public List<KeyValuePair<string, string>> Specifiers = new List<KeyValuePair<string, string>>();
        public bool HasGeneratedBody;
        public List<PropertyDecl> Properties = new List<PropertyDecl>();
        public string File;
        public int Line;
    }

    private class Scope
    {
        public string Namespace;
        public ClassDecl Class;
    }

    private readonly List<ClassDecl> classes_ = new List<ClassDecl>();
    private readonly List<EnumDecl> enums_ = new List<EnumDecl>();
    private readonly List<string> reflectedHeaders_ = new List<string>();

    private static readonly HashSet<string> kFlagSpecifiers = new HashSet<string> { "Edit", "Visible", "Transient", "Color", "EnumAsInt", "Angle", "Required" };
    private static readonly HashSet<string> kStringSpecifiers = new HashSet<string> { "Category", "Tooltip", "DisplayName", "Asset", "ActorWith" };
    private static readonly HashSet<string> kNumberSpecifiers = new HashSet<string> { "ClampMin", "ClampMax", "Speed" };

    public override bool Execute()
    {
        foreach (ITaskItem item in Headers)
        {
            string path = item.ItemSpec;
            if (!File.Exists(path)) continue;
            string text = File.ReadAllText(path);
            if (text.IndexOf("TCLASS(", StringComparison.Ordinal) < 0 && text.IndexOf("TSTRUCT(", StringComparison.Ordinal) < 0
                && text.IndexOf("TENUM(", StringComparison.Ordinal) < 0) continue;
            ParseHeader(path, text);
        }

        if (Log.HasLoggedErrors) return false;

        string generated = Generate();
        if (Log.HasLoggedErrors) return false;

        string directory = Path.GetDirectoryName(OutputFile);
        if (!string.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);
        // 中身が同じなら書かない（タイムスタンプが変わると毎回再コンパイルになる）
        if (!File.Exists(OutputFile) || File.ReadAllText(OutputFile) != generated)
        {
            File.WriteAllText(OutputFile, generated, new UTF8Encoding(false));
        }
        return true;
    }

    //==================================================================
    //字句解析
    //==================================================================

    // コメントとプリプロセッサの行を空白にする（位置と改行は残す）。lineDepth には各行の #if の深さを入れる
    private static string StripCommentsAndDirectives(string src, out int[] lineDepth)
    {
        char[] buf = src.ToCharArray();
        int i = 0;
        while (i < buf.Length)
        {
            char c = buf[i];
            if (c == '/' && i + 1 < buf.Length && buf[i + 1] == '/')
            {
                while (i < buf.Length && buf[i] != '\n') { buf[i] = ' '; i++; }
            }
            else if (c == '/' && i + 1 < buf.Length && buf[i + 1] == '*')
            {
                while (i < buf.Length && !(buf[i] == '*' && i + 1 < buf.Length && buf[i + 1] == '/'))
                {
                    if (buf[i] != '\n') buf[i] = ' ';
                    i++;
                }
                if (i < buf.Length) { buf[i] = ' '; buf[i + 1] = ' '; i += 2; }
            }
            else if (c == '"' || c == '\'')
            {
                i = SkipLiteral(src, i);
            }
            else
            {
                i++;
            }
        }

        string stripped = new string(buf);
        string[] lines = stripped.Split('\n');
        lineDepth = new int[lines.Length];
        int depth = 0;
        bool continued = false;
        int offset = 0;
        for (int line = 0; line < lines.Length; line++)
        {
            string l = lines[line];
            string trimmed = l.TrimStart();
            bool isDirective = continued || trimmed.StartsWith("#");
            if (isDirective && !continued)
            {
                string directive = trimmed.Substring(1).TrimStart();
                if (directive.StartsWith("if")) depth++;
                else if (directive.StartsWith("endif")) depth--;
            }
            lineDepth[line] = depth;
            if (isDirective)
            {
                continued = l.TrimEnd('\r', ' ', '\t').EndsWith("\\");
                for (int k = 0; k < l.Length; k++)
                {
                    if (l[k] != '\r') buf[offset + k] = ' ';
                }
            }
            offset += l.Length + 1;
        }
        return new string(buf);
    }

    // text[i] が引用符の位置。閉じ引用符の次の位置を返す
    private static int SkipLiteral(string text, int i)
    {
        char quote = text[i];
        i++;
        while (i < text.Length && text[i] != quote && text[i] != '\n')
        {
            if (text[i] == '\\') i++;
            i++;
        }
        return i + 1;
    }

    private static List<Token> Tokenize(string code)
    {
        var tokens = new List<Token>();
        int i = 0;
        while (i < code.Length)
        {
            char c = code[i];
            if (char.IsWhiteSpace(c)) { i++; continue; }
            int start = i;
            TokenKind kind;
            if (char.IsLetter(c) || c == '_')
            {
                while (i < code.Length && (char.IsLetterOrDigit(code[i]) || code[i] == '_')) i++;
                kind = TokenKind.Identifier;
            }
            else if (char.IsDigit(c))
            {
                while (i < code.Length && (char.IsLetterOrDigit(code[i]) || code[i] == '.' || code[i] == '_' || code[i] == '\'')) i++;
                kind = TokenKind.Number;
            }
            else if (c == '"' || c == '\'')
            {
                i = Math.Min(SkipLiteral(code, i), code.Length);
                kind = TokenKind.Literal;
            }
            else if (c == ':' && i + 1 < code.Length && code[i + 1] == ':')
            {
                i += 2;
                kind = TokenKind.Punct;
            }
            else
            {
                i++;
                kind = TokenKind.Punct;
            }
            tokens.Add(new Token { Kind = kind, Text = code.Substring(start, i - start), Start = start, End = i });
        }
        return tokens;
    }

    //==================================================================
    //構文解析
    //==================================================================

    private void ParseHeader(string path, string text)
    {
        int[] lineDepth;
        string code = StripCommentsAndDirectives(text, out lineDepth);
        List<Token> tokens = Tokenize(code);
        int[] lineStarts = ComputeLineStarts(text);
        // 先頭の #ifndef はインクルードガードとみなす
        int guardDepth = text.TrimStart().StartsWith("#ifndef") ? 1 : 0;

        var scopes = new Stack<Scope>();
        bool reflected = false;
        int i = 0;
        while (i < tokens.Count)
        {
            Token t = tokens[i];
            int line = LineOf(lineStarts, t.Start);
            Scope current = scopes.Count > 0 ? scopes.Peek() : null;

            if (t.Text == "TCLASS" || t.Text == "TSTRUCT" || t.Text == "TENUM" || t.Text == "TPROPERTY")
            {
                if (lineDepth[line - 1] > guardDepth)
                {
                    Error(path, line, t.Text + " を #if の中に書けない（Release で参照先が無くなる）");
                }
            }

            if (t.Kind == TokenKind.Identifier && t.Text == "namespace")
            {
                int j = i + 1;
                var name = new StringBuilder();
                while (j < tokens.Count && tokens[j].Text != "{" && tokens[j].Text != ";" && tokens[j].Text != "=")
                {
                    name.Append(tokens[j].Text);
                    j++;
                }
                if (j < tokens.Count && tokens[j].Text == "{")
                {
                    scopes.Push(new Scope { Namespace = name.ToString() });
                    i = j + 1;
                    continue;
                }
            }
            else if (t.Text == "TCLASS" || t.Text == "TSTRUCT")
            {
                bool isStruct = t.Text == "TSTRUCT";
                int argsEnd = SkipParens(tokens, i + 1);
                int j = argsEnd;
                ClassDecl decl = ParseClassHead(path, tokens, ref j, lineStarts, t.Text);
                if (decl != null)
                {
                    decl.IsStruct = isStruct;
                    foreach (int[] item in SplitArguments(tokens, i + 1, argsEnd))
                    {
                        AddClassSpecifier(path, line, code, tokens, item[0], item[1], decl);
                    }
                    if (current != null && current.Class != null)
                    {
                        if (isStruct) decl.Owner = current.Class;
                        else Error(path, decl.Line, "TCLASS を他のクラスの中に書けない");
                    }
                    decl.Namespaces = scopes.Reverse().Where(s => !string.IsNullOrEmpty(s.Namespace)).Select(s => s.Namespace).ToList();
                    classes_.Add(decl);
                    reflected = true;
                    scopes.Push(new Scope { Class = decl });
                    i = j + 1;
                    continue;
                }
                i = j;
                continue;
            }
            else if (t.Text == "TENUM")
            {
                int j = SkipParens(tokens, i + 1);
                EnumDecl decl = ParseEnum(path, tokens, ref j, lineStarts);
                if (decl != null)
                {
                    decl.Owner = current != null ? current.Class : null;
                    enums_.Add(decl);
                    reflected = true;
                }
                i = j;
                continue;
            }
            else if (t.Text == "TPROPERTY")
            {
                int argsEnd = SkipParens(tokens, i + 1);
                if (current == null || current.Class == null)
                {
                    Error(path, line, "TPROPERTY は TCLASS / TSTRUCT の本体の直下に書く");
                    i = argsEnd;
                    continue;
                }
                PropertyDecl prop = ParseProperty(path, code, tokens, i + 1, argsEnd, line, out int next);
                if (prop != null) current.Class.Properties.Add(prop);
                i = next;
                continue;
            }
            else if (t.Text == "GENERATED_BODY" || t.Text == "GENERATED_STRUCT_BODY")
            {
                if (current != null && current.Class != null)
                {
                    bool wantsStruct = t.Text == "GENERATED_STRUCT_BODY";
                    if (wantsStruct != current.Class.IsStruct)
                    {
                        Error(path, line, current.Class.IsStruct ? "TSTRUCT には GENERATED_STRUCT_BODY() を書く" : "TCLASS には GENERATED_BODY() を書く");
                    }
                    current.Class.HasGeneratedBody = true;
                }
            }
            else if (t.Text == "{")
            {
                scopes.Push(new Scope());
            }
            else if (t.Text == "}")
            {
                if (scopes.Count > 0) scopes.Pop();
            }
            i++;
        }

        if (reflected) reflectedHeaders_.Add(Path.GetFullPath(path).Replace('\\', '/'));
    }

    // tokens[start] が "(" なら対応する ")" の次の位置を返す
    private static int SkipParens(List<Token> tokens, int start)
    {
        if (start >= tokens.Count || tokens[start].Text != "(") return start;
        int depth = 0;
        for (int j = start; j < tokens.Count; j++)
        {
            if (tokens[j].Text == "(") depth++;
            else if (tokens[j].Text == ")")
            {
                depth--;
                if (depth == 0) return j + 1;
            }
        }
        return tokens.Count;
    }

    // かっこの中をトップレベルのカンマで区切り、各項目の [最初のトークン, 最後のトークンの次] を返す。argsStart は "("、argsEnd は ")" の次
    private static List<int[]> SplitArguments(List<Token> tokens, int argsStart, int argsEnd)
    {
        var items = new List<int[]>();
        int itemStart = argsStart + 1;
        int depth = 0;
        for (int j = argsStart + 1; j < argsEnd; j++)
        {
            string s = tokens[j].Text;
            bool last = j == argsEnd - 1;
            if (s == "(" || s == "{" || s == "[") depth++;
            else if ((s == ")" || s == "}" || s == "]") && !last) depth--;
            if ((depth == 0 && s == ",") || last)
            {
                if (j > itemStart) items.Add(new[] { itemStart, j });
                itemStart = j + 1;
            }
        }
        return items;
    }

    // TCLASS(名前 = 値) の 1 項目を読む
    private void AddClassSpecifier(string path, int line, string code, List<Token> tokens, int start, int end, ClassDecl decl)
    {
        string key = tokens[start].Text;
        if (decl.IsStruct)
        {
            Error(path, line, "TSTRUCT に指定子は付けられない");
            return;
        }
        if (end - start < 3 || tokens[start + 1].Text != "=")
        {
            Error(path, line, "TCLASS の指定子は「名前 = 値」で書く: " + code.Substring(tokens[start].Start, tokens[end - 1].End - tokens[start].Start));
            return;
        }
        string value = code.Substring(tokens[start + 2].Start, tokens[end - 1].End - tokens[start + 2].Start).Trim();
        if (key == "DisplayName" || key == "Category" || key == "Requires")
        {
            if (!value.StartsWith("\"")) Error(path, line, key + " には文字列を書く（例: " + key + " = \"...\"）");
        }
        else if (key != "Color" && key != "Order")
        {
            Error(path, line, "知らない TCLASS の指定子: " + key);
            return;
        }
        decl.Specifiers.Add(new KeyValuePair<string, string>(key, value));
    }

    // class/struct 名 [final] [: 継承] { まで読む。成功すると j は "{" を指す
    private ClassDecl ParseClassHead(string path, List<Token> tokens, ref int j, int[] lineStarts, string marker)
    {
        if (j >= tokens.Count || (tokens[j].Text != "class" && tokens[j].Text != "struct"))
        {
            Error(path, LineOf(lineStarts, tokens[Math.Min(j, tokens.Count - 1)].Start), marker + "() の後に class か struct が必要");
            return null;
        }
        int line = LineOf(lineStarts, tokens[j].Start);
        j++;
        if (j >= tokens.Count || tokens[j].Kind != TokenKind.Identifier)
        {
            Error(path, line, marker + " のクラス名が読めない");
            return null;
        }
        var decl = new ClassDecl { Name = tokens[j].Text, File = path, Line = line };
        j++;
        if (j < tokens.Count && tokens[j].Text == "final") j++;
        if (j < tokens.Count && tokens[j].Text == ":")
        {
            j++;
            while (j < tokens.Count && (tokens[j].Text == "public" || tokens[j].Text == "protected" || tokens[j].Text == "private" || tokens[j].Text == "virtual")) j++;
            var baseName = new StringBuilder();
            int angle = 0;
            while (j < tokens.Count && !(angle == 0 && (tokens[j].Text == "," || tokens[j].Text == "{")))
            {
                if (tokens[j].Text == "<") angle++;
                else if (tokens[j].Text == ">") angle--;
                baseName.Append(tokens[j].Text);
                j++;
            }
            decl.Base = baseName.ToString();
            while (j < tokens.Count && tokens[j].Text != "{") j++;
        }
        if (j >= tokens.Count || tokens[j].Text != "{")
        {
            Error(path, line, marker + " " + decl.Name + " の本体が見つからない");
            return null;
        }
        if (marker == "TSTRUCT")
        {
            // ponytail: 基底の Reflect を呼ぶ仕組みが要るので、継承が必要になったら足す
            if (!string.IsNullOrEmpty(decl.Base)) Error(path, line, "TSTRUCT " + decl.Name + " は継承できない");
        }
        else if (string.IsNullOrEmpty(decl.Base))
        {
            Error(path, line, "TCLASS " + decl.Name + " は TObject かその派生を継承する");
            return null;
        }
        return decl;
    }

    // enum [class] 名 [: 型] { 列挙子 } を読む。終わると j は "}" の次
    private EnumDecl ParseEnum(string path, List<Token> tokens, ref int j, int[] lineStarts)
    {
        int line = LineOf(lineStarts, tokens[Math.Min(j, tokens.Count - 1)].Start);
        if (j >= tokens.Count || tokens[j].Text != "enum")
        {
            Error(path, line, "TENUM() の後に enum が必要");
            return null;
        }
        j++;
        if (j < tokens.Count && (tokens[j].Text == "class" || tokens[j].Text == "struct")) j++;
        if (j >= tokens.Count || tokens[j].Kind != TokenKind.Identifier)
        {
            Error(path, line, "TENUM の名前が読めない");
            return null;
        }
        var decl = new EnumDecl { Name = tokens[j].Text };
        while (j < tokens.Count && tokens[j].Text != "{") j++;
        j++;
        bool expectName = true;
        int depth = 0;
        while (j < tokens.Count && !(depth == 0 && tokens[j].Text == "}"))
        {
            string s = tokens[j].Text;
            if (s == "(" || s == "{") depth++;
            else if (s == ")" || s == "}") depth--;
            else if (depth == 0 && s == ",") expectName = true;
            else if (expectName && tokens[j].Kind == TokenKind.Identifier)
            {
                decl.Enumerators.Add(s);
                expectName = false;
            }
            j++;
        }
        j++;
        return decl;
    }

    // TPROPERTY(指定子) の後のメンバー宣言を ";" まで読む
    private PropertyDecl ParseProperty(string path, string code, List<Token> tokens, int argsStart, int argsEnd, int line, out int next)
    {
        var prop = new PropertyDecl { Line = line };

        // 指定子: 各項目は「名前」か「名前 = 値」
        foreach (int[] item in SplitArguments(tokens, argsStart, argsEnd))
        {
            AddSpecifier(path, line, code, tokens, item[0], item[1], prop);
        }

        // 宣言: 最初のトップレベルの "=" か "{" が初期化子の始まり。その直前の識別子がメンバー名
        int declStart = argsEnd;
        int k = declStart;
        int initStart = -1;
        int depth = 0;
        while (k < tokens.Count)
        {
            string s = tokens[k].Text;
            if (depth == 0 && s == ";") break;
            if (depth == 0 && initStart < 0 && (s == "=" || s == "{")) initStart = k;
            if (s == "(" || s == "{" || s == "[") depth++;
            else if (s == ")" || s == "}" || s == "]") depth--;
            if (depth == 0 && initStart < 0 && s == "[")
            {
                Error(path, line, "TPROPERTY に配列は使えない");
            }
            k++;
        }
        next = k + 1;
        int nameEnd = initStart >= 0 ? initStart : k;
        int nameIndex = -1;
        for (int m = nameEnd - 1; m >= declStart; m--)
        {
            if (tokens[m].Kind == TokenKind.Identifier) { nameIndex = m; break; }
        }
        if (nameIndex <= declStart)
        {
            Error(path, line, "TPROPERTY の後のメンバー宣言が読めない");
            return null;
        }
        for (int m = declStart; m < nameIndex; m++)
        {
            if (tokens[m].Text == "static" || tokens[m].Text == "constexpr")
            {
                Error(path, line, "TPROPERTY を static メンバーに付けられない");
                return null;
            }
        }
        prop.Member = tokens[nameIndex].Text;
        prop.TypeText = NormalizeSpaces(code.Substring(tokens[declStart].Start, tokens[nameIndex - 1].End - tokens[declStart].Start));
        if (prop.Specifiers.Any(s => s.Key == "Asset") && ElementType(prop.TypeText) != "std::string")
        {
            Error(path, line, "Asset は std::string（か、その std::vector）にだけ付けられる");
        }
        if (prop.Specifiers.Any(s => s.Key == "ActorWith") && ElementType(prop.TypeText) != "ActorRef" && ElementType(prop.TypeText) != "Tako::ActorRef")
        {
            Error(path, line, "ActorWith は ActorRef（か、その std::vector）にだけ付けられる");
        }
        return prop;
    }

    private void AddSpecifier(string path, int line, string code, List<Token> tokens, int start, int end, PropertyDecl prop)
    {
        string key = tokens[start].Text;
        string value = null;
        if (end - start >= 3 && tokens[start + 1].Text == "=")
        {
            value = code.Substring(tokens[start + 2].Start, tokens[end - 1].End - tokens[start + 2].Start).Trim();
        }
        else if (end - start != 1)
        {
            Error(path, line, "TPROPERTY の指定子が読めない: " + code.Substring(tokens[start].Start, tokens[end - 1].End - tokens[start].Start));
            return;
        }

        if (kFlagSpecifiers.Contains(key))
        {
            if (value != null) Error(path, line, key + " は値を取らない");
        }
        else if (kStringSpecifiers.Contains(key))
        {
            if (value == null || !value.StartsWith("\"")) Error(path, line, key + " には文字列を書く（例: " + key + " = \"...\"）");
        }
        else if (kNumberSpecifiers.Contains(key))
        {
            if (value == null) Error(path, line, key + " には値を書く（例: " + key + " = 0）");
        }
        else
        {
            Error(path, line, "知らない TPROPERTY の指定子: " + key);
            return;
        }
        prop.Specifiers.Add(new KeyValuePair<string, string>(key, value));
    }

    //==================================================================
    //生成
    //==================================================================

    private string Generate()
    {
        var sb = new StringBuilder();
        sb.AppendLine("// TakoHeaderTool が生成したファイル。手で編集しない");
        sb.AppendLine("#include \"TypeRegistry.h\"");
        sb.AppendLine("#include \"PropertyVisitor.h\"");
        foreach (string header in reflectedHeaders_.Distinct())
        {
            sb.AppendLine("#include \"" + header + "\"");
        }
        sb.AppendLine();

        foreach (ClassDecl c in classes_)
        {
            if (!c.HasGeneratedBody)
            {
                Error(c.File, c.Line, c.IsStruct
                    ? "TSTRUCT " + c.Name + " の本体に GENERATED_STRUCT_BODY() が無い"
                    : "TCLASS " + c.Name + " の本体に GENERATED_BODY() が無い");
                continue;
            }
            if (c.IsStruct) GenerateStruct(sb, c);
            else GenerateClass(sb, c);
        }
        List<ClassDecl> registered = classes_.Where(c => !c.IsStruct).ToList();

        sb.AppendLine("namespace Tako {");
        sb.AppendLine();
        sb.AppendLine("  void RegisterTypes_" + ModuleName + "()");
        sb.AppendLine("  {");
        if (registered.Count > 0)
        {
            sb.AppendLine("    TypeRegistry* registry = TypeRegistry::GetInstance();");
            foreach (ClassDecl c in registered)
            {
                sb.AppendLine("    registry->Register(" + QualifiedName(c) + "::StaticType());");
            }
        }
        sb.AppendLine("  }");
        sb.AppendLine();
        sb.AppendLine("} // namespace Tako");
        return sb.ToString().Replace("\r\n", "\n");
    }

    private void GenerateClass(StringBuilder sb, ClassDecl c)
    {
        foreach (string ns in c.Namespaces) sb.AppendLine("namespace " + ns + " {");
        sb.AppendLine();
        sb.AppendLine("const ::Tako::TypeInfo& " + c.Name + "::StaticType()");
        sb.AppendLine("{");
        Func<string, string, string> specifier = (key, fallback) =>
        {
            var found = c.Specifiers.Where(s => s.Key == key).ToList();
            return found.Count > 0 ? found[found.Count - 1].Value : fallback;
        };
        sb.AppendLine("  static const ::Tako::TypeInfo type{ \"" + c.Name + "\", &" + c.Base + "::StaticType(), ::Tako::MakeTypeCreator<" + c.Name + ">(), "
            + specifier("DisplayName", "\"\"") + ", " + specifier("Category", "\"\"") + ", " + specifier("Color", "0") + ", " + specifier("Requires", "\"\"") + ", " + specifier("Order", "0") + " };");
        sb.AppendLine("  return type;");
        sb.AppendLine("}");
        sb.AppendLine();
        sb.AppendLine("const ::Tako::TypeInfo& " + c.Name + "::GetType() const");
        sb.AppendLine("{");
        sb.AppendLine("  return StaticType();");
        sb.AppendLine("}");
        sb.AppendLine();
        sb.AppendLine("void " + c.Name + "::Reflect(::Tako::PropertyVisitor& v)");
        sb.AppendLine("{");
        sb.AppendLine("  " + c.Base + "::Reflect(v);");
        foreach (PropertyDecl p in c.Properties)
        {
            GenerateProperty(sb, c, p);
        }
        sb.AppendLine("}");
        sb.AppendLine();
        foreach (string ns in c.Namespaces) sb.AppendLine("} // namespace " + ns);
        sb.AppendLine();
    }

    private void GenerateStruct(StringBuilder sb, ClassDecl c)
    {
        foreach (string ns in c.Namespaces) sb.AppendLine("namespace " + ns + " {");
        sb.AppendLine();
        sb.AppendLine("void " + NestedName(c) + "::Reflect(::Tako::PropertyVisitor& v)");
        sb.AppendLine("{");
        if (c.Properties.Count == 0) sb.AppendLine("  (void)v;");
        foreach (PropertyDecl p in c.Properties)
        {
            GenerateProperty(sb, c, p);
        }
        sb.AppendLine("}");
        sb.AppendLine();
        foreach (string ns in c.Namespaces) sb.AppendLine("} // namespace " + ns);
        sb.AppendLine();
    }

    // 名前空間の中での名前（入れ子の TSTRUCT は Outer::Inner）
    private static string NestedName(ClassDecl c)
    {
        return c.Owner != null ? NestedName(c.Owner) + "::" + c.Name : c.Name;
    }

    // std::vector<X> なら X、それ以外はそのまま
    private static string ElementType(string typeText)
    {
        const string vectorPrefix = "std::vector";
        if (!typeText.StartsWith(vectorPrefix, StringComparison.Ordinal)) return typeText;
        string rest = typeText.Substring(vectorPrefix.Length).Trim();
        if (!rest.StartsWith("<") || !rest.EndsWith(">")) return typeText;
        return rest.Substring(1, rest.Length - 2).Trim();
    }

    private void GenerateProperty(StringBuilder sb, ClassDecl c, PropertyDecl p)
    {
        string key = p.Member.EndsWith("_") ? p.Member.Substring(0, p.Member.Length - 1) : p.Member;
        string elementType = ElementType(p.TypeText);
        bool isBool = elementType == "bool";

        string displayName = null;
        string category = null;
        string tooltip = null;
        string min = null;
        string max = null;
        string speed = null;
        string asset = null;
        string actorWith = null;
        var flags = new List<string>();
        foreach (KeyValuePair<string, string> s in p.Specifiers)
        {
            switch (s.Key)
            {
                case "DisplayName": displayName = s.Value; break;
                case "Category": category = s.Value; break;
                case "Tooltip": tooltip = s.Value; break;
                case "ClampMin": min = s.Value; break;
                case "ClampMax": max = s.Value; break;
                case "Speed": speed = s.Value; break;
                case "Asset": asset = s.Value; break;
                case "ActorWith": actorWith = s.Value; break;
                default: flags.Add("::Tako::kProp" + s.Key); break;
            }
        }
        if (displayName == null) displayName = "\"" + MakeDisplayName(key, isBool) + "\"";

        EnumDecl enumDecl = FindEnum(c, p, elementType);
        if (enumDecl != null)
        {
            sb.AppendLine("  static constexpr ::Tako::EnumEntry kEnumEntries_" + key + "[] = {");
            foreach (string e in enumDecl.Enumerators)
            {
                sb.AppendLine("    { \"" + e + "\", static_cast<int64_t>(" + elementType + "::" + e + ") },");
            }
            sb.AppendLine("  };");
            sb.AppendLine("  static constexpr ::Tako::EnumInfo kEnumInfo_" + key + "{ kEnumEntries_" + key + " };");
        }

        var fields = new List<string>();
        fields.Add(".displayName = " + displayName);
        if (category != null) fields.Add(".category = " + category);
        if (tooltip != null) fields.Add(".tooltip = " + tooltip);
        if (min != null) fields.Add(".min = static_cast<double>(" + min + ")");
        if (max != null) fields.Add(".max = static_cast<double>(" + max + ")");
        if (speed != null) fields.Add(".speed = static_cast<float>(" + speed + ")");
        if (flags.Count > 0) fields.Add(".flags = " + string.Join(" | ", flags));
        if (enumDecl != null) fields.Add(".enumInfo = &kEnumInfo_" + key);
        if (asset != null) fields.Add(".asset = " + asset);
        if (actorWith != null) fields.Add(".actorWith = " + actorWith);
        sb.AppendLine("  v(\"" + key + "\", " + p.Member + ", { " + string.Join(", ", fields) + " });");
    }

    // 型の最後の識別子が同じ名前の TENUM を探す。同じ名前が複数あれば、同じクラスの中で宣言したものを使う
    private EnumDecl FindEnum(ClassDecl c, PropertyDecl p, string elementType)
    {
        string simpleName = elementType;
        int colon = simpleName.LastIndexOf("::", StringComparison.Ordinal);
        if (colon >= 0) simpleName = simpleName.Substring(colon + 2);
        List<EnumDecl> candidates = enums_.Where(e => e.Name == simpleName).ToList();
        if (candidates.Count == 0) return null;
        if (candidates.Count == 1) return candidates[0];
        List<EnumDecl> own = candidates.Where(e => e.Owner == c).ToList();
        if (own.Count == 1) return own[0];
        Error(c.File, p.Line, "TENUM " + simpleName + " が複数あり、どれか決められない");
        return null;
    }

    private static string QualifiedName(ClassDecl c)
    {
        var parts = new List<string>(c.Namespaces);
        parts.Add(c.Name);
        return "::" + string.Join("::", parts);
    }

    // chargeTime → Charge Time、launchSpreadXZ → Launch Spread XZ。bool の先頭の is は外す
    private static string MakeDisplayName(string key, bool isBool)
    {
        string k = key.Replace('_', ' ').Trim();
        if (isBool && k.Length > 2 && k.StartsWith("is") && char.IsUpper(k[2])) k = k.Substring(2);
        var sb = new StringBuilder();
        for (int i = 0; i < k.Length; i++)
        {
            char c = k[i];
            if (i > 0 && char.IsUpper(c))
            {
                char prev = k[i - 1];
                bool nextLower = i + 1 < k.Length && char.IsLower(k[i + 1]);
                if (char.IsLower(prev) || char.IsDigit(prev) || (char.IsUpper(prev) && nextLower)) sb.Append(' ');
            }
            sb.Append(i == 0 ? char.ToUpperInvariant(c) : c);
        }
        return sb.ToString();
    }

    //==================================================================
    //補助
    //==================================================================

    private static string NormalizeSpaces(string s)
    {
        var sb = new StringBuilder();
        bool space = false;
        foreach (char c in s.Trim())
        {
            if (char.IsWhiteSpace(c)) { space = true; continue; }
            if (space && sb.Length > 0) sb.Append(' ');
            space = false;
            sb.Append(c);
        }
        return sb.ToString();
    }

    private static int[] ComputeLineStarts(string text)
    {
        var starts = new List<int> { 0 };
        for (int i = 0; i < text.Length; i++)
        {
            if (text[i] == '\n') starts.Add(i + 1);
        }
        return starts.ToArray();
    }

    // 1 始まりの行番号
    private static int LineOf(int[] lineStarts, int position)
    {
        int index = Array.BinarySearch(lineStarts, position);
        return index >= 0 ? index + 1 : ~index;
    }

    private void Error(string file, int line, string message)
    {
        Log.LogError(null, "TAKO001", null, file, line, 0, 0, 0, message);
    }
}
