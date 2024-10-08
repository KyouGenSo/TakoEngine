#include<Windows.h>
#include<wrl.h>
#include<cstdint>
#include<string>
#include<fstream>
#include<sstream>
#include<format>
#include <unordered_map>
#include <cassert>
#include <vector>

#include <d3d12.h>
#include <dxgi1_6.h>
#include<dxcapi.h>
#include <dxgidebug.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "dxguid.lib")

#include"externals/DirectXTex/d3dx12.h"
#include"externals/DirectXTex/DirectXTex.h"

#include"externals/imgui/imgui.h"
#include"externals/imgui/imgui_impl_win32.h"
#include"externals/imgui/imgui_impl_dx12.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include "WinApp.h"
#include "DX12Basic.h"
#include"Input.h"
#include "SpriteBasic.h"
#include "Sprite.h"
#include"D3DResourceLeakCheker.h"
#include"TextureManager.h"

#include "xaudio2.h"
#pragma comment(lib, "xaudio2.lib")

#include "Logger.h"
#include "StringUtility.h"

// Function includes
#include"Vector4.h"
#include"Vector2.h"
#include"Mat4x4Func.h"
#include"Vec3Func.h"

#define PI 3.14159265359f

// ComPtrのエイリアス
template<class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;


struct VertexData
{
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

struct VertexDataNoTex
{
	Vector4 position;
	Vector3 normal;
};

struct VertexHash {
	size_t operator()(const VertexData& vertex) const {
		size_t h1 = std::hash<float>{}(vertex.position.x);
		size_t h2 = std::hash<float>{}(vertex.position.y);
		size_t h3 = std::hash<float>{}(vertex.position.z);
		size_t h4 = std::hash<float>{}(vertex.position.w);
		size_t h5 = std::hash<float>{}(vertex.texcoord.x);
		size_t h6 = std::hash<float>{}(vertex.texcoord.y);
		return h1 ^ h2 ^ h3 ^ h4 ^ h5 ^ h6;
	}
};

struct VertexEqual {
	bool operator()(const VertexData& lhs, const VertexData& rhs) const {
		return lhs.position.x == rhs.position.x &&
			lhs.position.y == rhs.position.y &&
			lhs.position.z == rhs.position.z &&
			lhs.position.w == rhs.position.w &&
			lhs.texcoord.x == rhs.texcoord.x &&
			lhs.texcoord.y == rhs.texcoord.y;
	}
};

struct Material
{
	Vector4 color;
	bool enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};

struct TransformationMatrix
{
	Matrix4x4 WVP;
	Matrix4x4 world;
};

struct DirectionalLight
{
	Vector4 color;
	Vector3 direction;
	int32_t lightType;
	float intensity;
};

struct MaterialData {
	std::string texturePath;
};

struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};

struct ModelDataNoTex {
	std::vector<VertexDataNoTex> vertices;
};

struct ChunkHeader {
	char id[4]; // チャンクのID
	uint32_t size; // チャンクのサイズ
};

struct RiffHeader {
	ChunkHeader chunk; // "RIFF"
	char type[4]; // "WAVE"
};

struct FormatChunk {
	ChunkHeader chunk; // "fmt "
	WAVEFORMATEX fmt; // 波形フォーマット
};

struct SoundData {
	// 波形フォーマット
	WAVEFORMATEX wfex;
	// バッファの先頭アドレス
	BYTE* pBuffer;
	// バッファのサイズ
	unsigned int bufferSize;
};

//-----------------------------------------FUNCTION-----------------------------------------//

ModelData LoadObjFile(const std::string& directoryPath, const std::string& fileName);

MaterialData LoadMtlFile(const std::string& directoryPath, const std::string& fileName);

std::vector<ModelData> LoadMutiMeshObjFile(const std::string& directoryPath, const std::string& fileName);

std::vector<ModelData> LoadMutiMaterialFile(const std::string& directoryPath, const std::string& fileName);

std::unordered_map<std::string, MaterialData> LoadMutiMaterialMtlFile(const std::string& directoryPath, const std::string& fileName);

ModelDataNoTex LoadObjFileNoTex(const std::string& directoryPath, const std::string& fileName);

SoundData LoadWaveFile(const char* filename);

void SoundUnload(SoundData* soundData);

void SoundPlay(IXAudio2* xAudio2, const SoundData& soundData);

//-----------------------------------------FUNCTION-----------------------------------------//


//Windowsプログラムのエントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// リソースリークチェッカー
	D3DResourceLeakCheker d3dResourceLeakCheker;

	//------------------------------------------WINDOW------------------------------------------

	//ウィンドウクラスの初期化
	WinApp* winApp = new WinApp();
	winApp->Initialize();

	//-----------------------------------------WINDOW-----------------------------------------//

	//-----------------------------------------汎用機能初期化-----------------------------------------
	HRESULT hr;

	Input* input = new Input();
	input->Initialize(winApp);

	//XAudio2
	ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice;

	// XAudio2の初期化
	hr = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	hr = xAudio2->CreateMasteringVoice(&masterVoice);

	// サウンドの読み込み
	SoundData soundData = LoadWaveFile("resources/fanfare.wav");

	//-----------------------------------------汎用機能初期化-----------------------------------------//


	//-----------------------------------------基盤システムの初期化-----------------------------------------

	// DX12の初期化
	DX12Basic* dx12 = new DX12Basic();
	dx12->Initialize(winApp);

	// TextureManagerの初期化
	TextureManager::GetInstance()->Initialize(dx12);

	// Sprite共通クラスの初期化
	SpriteBasic* spriteBasic = new SpriteBasic();
	spriteBasic->Initialize(dx12);

	//-----------------------------------------基盤システムの初期化-----------------------------------------//


	//-----------------------------------------PSO-----------------------------------------

	// textureがない場合のPSOを生成---------------------------------------------------------

	// rootSignatureの生成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignatureNoTex{};
	descriptionRootSignatureNoTex.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameterの設定。複数設定できるので配列
	D3D12_ROOT_PARAMETER rootParametersNoTex[3] = {};

	// WVP行列.VertexShaderで使う
	rootParametersNoTex[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParametersNoTex[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
	rootParametersNoTex[0].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

	// materialの設定.PixelShaderで使う
	rootParametersNoTex[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParametersNoTex[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParametersNoTex[1].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

	// DirectionalLightの設定.PixelShaderで使う
	rootParametersNoTex[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParametersNoTex[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParametersNoTex[2].Descriptor.ShaderRegister = 1; // レジスタ番号とバインド

	descriptionRootSignatureNoTex.pParameters = rootParametersNoTex;
	descriptionRootSignatureNoTex.NumParameters = _countof(rootParametersNoTex);

	ComPtr<ID3DBlob> signatureBlobNoTex = nullptr;
	ComPtr<ID3DBlob> errorBlobNoTex = nullptr;

	hr = D3D12SerializeRootSignature(&descriptionRootSignatureNoTex, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlobNoTex, &errorBlobNoTex);
	if (FAILED(hr))
	{
		Logger::Log(reinterpret_cast<char*>(errorBlobNoTex->GetBufferPointer()));
		assert(false);
	}

	ComPtr<ID3D12RootSignature> rootSignatureNoTex = nullptr;
	hr = dx12->GetDevice()->CreateRootSignature(0, signatureBlobNoTex->GetBufferPointer(), signatureBlobNoTex->GetBufferSize(), IID_PPV_ARGS(rootSignatureNoTex.GetAddressOf()));
	signatureBlobNoTex->GetBufferSize(), IID_PPV_ARGS(rootSignatureNoTex.GetAddressOf());
	assert(SUCCEEDED(hr));

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescsNoTex[2] = {};
	inputElementDescsNoTex[0].SemanticName = "POSITION";
	inputElementDescsNoTex[0].SemanticIndex = 0;
	inputElementDescsNoTex[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescsNoTex[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescsNoTex[1].SemanticName = "NORMAL";
	inputElementDescsNoTex[1].SemanticIndex = 0;
	inputElementDescsNoTex[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescsNoTex[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDescNoTex{};
	inputLayoutDescNoTex.pInputElementDescs = inputElementDescsNoTex;
	inputLayoutDescNoTex.NumElements = _countof(inputElementDescsNoTex);

	// BlendState
	D3D12_BLEND_DESC blendDescNoTex{};
	blendDescNoTex.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerState
	D3D12_RASTERIZER_DESC rasterizerDescNoTex{};
	// 三角形の中を塗りつぶす
	rasterizerDescNoTex.FillMode = D3D12_FILL_MODE_SOLID;
	// 裏面を表示しない
	rasterizerDescNoTex.CullMode = D3D12_CULL_MODE_BACK;

	// shaderのコンパイル
	ComPtr<IDxcBlob> vertexShaderNoTexBlob = dx12->CompileShader(L"resources/shaders/NoTex.VS.hlsl", L"vs_6_0");
	assert(vertexShaderNoTexBlob != nullptr);

	ComPtr<IDxcBlob> pixelShaderNoTexBlob = dx12->CompileShader(L"resources/shaders/NoTex.PS.hlsl", L"ps_6_0");
	assert(pixelShaderNoTexBlob != nullptr);

	// DepthStencilState
	D3D12_DEPTH_STENCIL_DESC depthStencilDescNoTex{};
	// depthの機能を有効化にする
	depthStencilDescNoTex.DepthEnable = true;
	// 書き込みします
	depthStencilDescNoTex.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 深度の比較方法
	depthStencilDescNoTex.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// PSOの生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateNoTexDesc{};
	graphicsPipelineStateNoTexDesc.pRootSignature = rootSignatureNoTex.Get();
	graphicsPipelineStateNoTexDesc.InputLayout = inputLayoutDescNoTex;
	graphicsPipelineStateNoTexDesc.VS = { vertexShaderNoTexBlob->GetBufferPointer(), vertexShaderNoTexBlob->GetBufferSize() };
	graphicsPipelineStateNoTexDesc.PS = { pixelShaderNoTexBlob->GetBufferPointer(), pixelShaderNoTexBlob->GetBufferSize() };
	graphicsPipelineStateNoTexDesc.BlendState = blendDescNoTex;
	graphicsPipelineStateNoTexDesc.RasterizerState = rasterizerDescNoTex;
	// 書き込むRTVの情報
	graphicsPipelineStateNoTexDesc.NumRenderTargets = 1;
	graphicsPipelineStateNoTexDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateNoTexDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定
	graphicsPipelineStateNoTexDesc.SampleDesc.Count = 1;
	graphicsPipelineStateNoTexDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// DepthStencilの設定
	graphicsPipelineStateNoTexDesc.DepthStencilState = depthStencilDescNoTex;
	graphicsPipelineStateNoTexDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 実際に生成
	ComPtr<ID3D12PipelineState> graphicsPipelineStateNoTex = nullptr;
	hr = dx12->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateNoTexDesc, IID_PPV_ARGS(&graphicsPipelineStateNoTex));
	assert(SUCCEEDED(hr));

	//-----------------------------------------PSO-----------------------------------------///


	//-------------------------------------------------------------------------------------------------------------------
	//                                                                                                                   
	//                                                  Resourceの作成                                                    
	//                                                                                                                   
	//-------------------------------------------------------------------------------------------------------------------

	//------------------------------------------------------Model------------------------------------------------------
	enum ModelType {
		Plane,
		Teapot,
		Bunny,
		MultiMesh,
		MultiMaterial,
		Suzanne,
	};

	ModelType modelType = Plane;

	// Planeのデータを読み込む------------------------------------------------------------
	ModelData planeData = LoadObjFile("resources", "plane.obj");

	// 頂点リソースを作る
	ComPtr<ID3D12Resource> planeVertexResource = dx12->MakeBufferResource(sizeof(VertexData) * planeData.vertices.size());

	// 頂点バッファビューを作る
	D3D12_VERTEX_BUFFER_VIEW planeVertexBufferView{};
	planeVertexBufferView.BufferLocation = planeVertexResource->GetGPUVirtualAddress();
	planeVertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * planeData.vertices.size());
	planeVertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	VertexData* planeVertexData = nullptr;
	planeVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&planeVertexData));
	memcpy(planeVertexData, planeData.vertices.data(), sizeof(VertexData) * planeData.vertices.size());


	// Teapotのデータを読み込む------------------------------------------------------------
	ModelData teapotData = LoadObjFile("resources", "teapot.obj");

	// 頂点リソースを作る
	ComPtr<ID3D12Resource> teapotVertexResource = dx12->MakeBufferResource(sizeof(VertexData) * teapotData.vertices.size());

	// 頂点バッファビューを作る
	D3D12_VERTEX_BUFFER_VIEW teapotVertexBufferView{};
	teapotVertexBufferView.BufferLocation = teapotVertexResource->GetGPUVirtualAddress();
	teapotVertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * teapotData.vertices.size());
	teapotVertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	VertexData* teapotVertexData = nullptr;
	teapotVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&teapotVertexData));
	memcpy(teapotVertexData, teapotData.vertices.data(), sizeof(VertexData) * teapotData.vertices.size());


	// Bunnyのデータを読み込む---------------------------------------------------------
	ModelData bunnyData = LoadObjFile("resources", "bunny.obj");

	// 頂点リソースを作る
	ComPtr<ID3D12Resource> bunnyVertexResource = dx12->MakeBufferResource(sizeof(VertexData) * bunnyData.vertices.size());

	// 頂点バッファビューを作る
	D3D12_VERTEX_BUFFER_VIEW bunnyVertexBufferView{};
	bunnyVertexBufferView.BufferLocation = bunnyVertexResource->GetGPUVirtualAddress();
	bunnyVertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * bunnyData.vertices.size());
	bunnyVertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	VertexData* bunnyVertexData = nullptr;
	bunnyVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&bunnyVertexData));
	memcpy(bunnyVertexData, bunnyData.vertices.data(), sizeof(VertexData) * bunnyData.vertices.size());


	// Mutimesh.objデータを読み込む---------------------------------------------------------
	std::vector<ModelData> multiMeshModelDatas = LoadMutiMeshObjFile("resources", "multiMesh.obj");

	std::vector<ComPtr<ID3D12Resource>> multiMeshVertexResources;
	std::vector<D3D12_VERTEX_BUFFER_VIEW> multiMeshVertexBufferViews;

	for (const auto& modelData : multiMeshModelDatas) {
		ComPtr<ID3D12Resource> vertexResource = dx12->MakeBufferResource(sizeof(VertexData) * modelData.vertices.size());
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
		vertexBufferView.StrideInBytes = sizeof(VertexData);

		VertexData* vertexData = nullptr;
		vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
		memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

		multiMeshVertexResources.push_back(vertexResource);
		multiMeshVertexBufferViews.push_back(vertexBufferView);
	}

	// MutiMaterial.objデータを読み込む---------------------------------------------------------
	std::vector<ModelData> multiMaterialModelDatas = LoadMutiMaterialFile("resources", "multiMaterial.obj");

	std::vector<ComPtr<ID3D12Resource>> multiMaterialVertexResources;
	std::vector<D3D12_VERTEX_BUFFER_VIEW> multiMaterialVertexBufferViews;

	for (const auto& modelData : multiMaterialModelDatas) {
		ComPtr<ID3D12Resource> vertexResource = dx12->MakeBufferResource(sizeof(VertexData) * modelData.vertices.size());
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
		vertexBufferView.StrideInBytes = sizeof(VertexData);

		VertexData* vertexData = nullptr;
		vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
		memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

		multiMaterialVertexResources.push_back(vertexResource);
		multiMaterialVertexBufferViews.push_back(vertexBufferView);
	}

	// Suzanneのデータを読み込む---------------------------------------------------------
	ModelDataNoTex suzanneData = LoadObjFileNoTex("resources", "suzanne.obj");

	// 頂点リソースを作る
	ComPtr<ID3D12Resource> suzanneVertexResource = dx12->MakeBufferResource(sizeof(VertexDataNoTex) * suzanneData.vertices.size());

	// 頂点バッファビューを作る
	D3D12_VERTEX_BUFFER_VIEW suzanneVertexBufferView{};
	suzanneVertexBufferView.BufferLocation = suzanneVertexResource->GetGPUVirtualAddress();
	suzanneVertexBufferView.SizeInBytes = UINT(sizeof(VertexDataNoTex) * suzanneData.vertices.size());
	suzanneVertexBufferView.StrideInBytes = sizeof(VertexDataNoTex);

	// 頂点リソースにデータを書き込む
	VertexDataNoTex* suzanneVertexData = nullptr;
	suzanneVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&suzanneVertexData));
	memcpy(suzanneVertexData, suzanneData.vertices.data(), sizeof(VertexDataNoTex) * suzanneData.vertices.size());

	// 球のリソースを作る----------------------------------------------------------------------------------------------
	const int kVertexCount = 16 * 16 * 6;

	ComPtr<ID3D12Resource> sphereVertexResource = dx12->MakeBufferResource(sizeof(VertexData) * kVertexCount);

	//VertexBufferView
	D3D12_VERTEX_BUFFER_VIEW sphereVertexBufferView{};
	// リソースの先頭アドレスから使う
	sphereVertexBufferView.BufferLocation = sphereVertexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点三つ分のサイズ
	sphereVertexBufferView.SizeInBytes = sizeof(VertexData) * kVertexCount;
	// 一つの頂点のサイズ
	sphereVertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	VertexData* sphereVertexData = nullptr;
	// アドレスを取得
	sphereVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereVertexData));

	// 球を作成(vertexIndex Version)-------------------------------------------------------------//
	ComPtr<ID3D12Resource> sphereIndexResource = dx12->MakeBufferResource(sizeof(uint32_t) * kVertexCount);
	D3D12_INDEX_BUFFER_VIEW sphereIndexBufferView{};
	sphereIndexBufferView.BufferLocation = sphereIndexResource->GetGPUVirtualAddress();
	sphereIndexBufferView.SizeInBytes = sizeof(uint32_t) * kVertexCount;
	sphereIndexBufferView.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* indexData = nullptr;
	sphereIndexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));


	const uint32_t kSubdivision = 16;
	const float kLonEvery = DirectX::XM_2PI / float(kSubdivision);
	const float kLatEvery = DirectX::XM_PI / float(kSubdivision);

	std::unordered_map<VertexData, uint32_t, VertexHash, VertexEqual> uniqueVertices;

	uint32_t uniqueVertexCount = 0;

	for (int latIndex = 0; latIndex < kSubdivision; latIndex++) {

		float theta = -DirectX::XM_PIDIV2 + kLatEvery * latIndex;

		for (int lonIndex = 0; lonIndex < kSubdivision; lonIndex++) {

			uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;
			float phi = kLonEvery * lonIndex;

			VertexData vertices[6];
			vertices[0] = { cos(theta) * cos(phi), sin(theta), cos(theta) * sin(phi), 1.0f, float(lonIndex) / kSubdivision, 1.0f - float(latIndex) / kSubdivision };
			vertices[1] = { cos(theta + kLatEvery) * cos(phi), sin(theta + kLatEvery), cos(theta + kLatEvery) * sin(phi), 1.0f, float(lonIndex) / kSubdivision, 1.0f - float(latIndex + 1) / kSubdivision };
			vertices[2] = { cos(theta) * cos(phi + kLonEvery), sin(theta), cos(theta) * sin(phi + kLonEvery), 1.0f, float(lonIndex + 1) / kSubdivision, 1.0f - float(latIndex) / kSubdivision };
			vertices[3] = { cos(theta) * cos(phi + kLonEvery), sin(theta), cos(theta) * sin(phi + kLonEvery), 1.0f, float(lonIndex + 1) / kSubdivision, 1.0f - float(latIndex) / kSubdivision };
			vertices[4] = { cos(theta + kLatEvery) * cos(phi), sin(theta + kLatEvery), cos(theta + kLatEvery) * sin(phi), 1.0f, float(lonIndex) / kSubdivision, 1.0f - float(latIndex + 1) / kSubdivision };
			vertices[5] = { cos(theta + kLatEvery) * cos(phi + kLonEvery), sin(theta + kLatEvery), cos(theta + kLatEvery) * sin(phi + kLonEvery), 1.0f, float(lonIndex + 1) / kSubdivision, 1.0f - float(latIndex + 1) / kSubdivision };

			for (int i = 0; i < 6; i++) {
				auto iter = uniqueVertices.find(vertices[i]);
				if (iter != uniqueVertices.end()) {
					indexData[start + i] = iter->second;
				} else {
					uniqueVertices[vertices[i]] = uniqueVertexCount;
					sphereVertexData[uniqueVertexCount] = vertices[i];
					indexData[start + i] = uniqueVertexCount;
					uniqueVertexCount++;
				}
			}
		}
	}

	for (int i = 0; i < kVertexCount; i++)
	{
		sphereVertexData[i].normal = Vector3(sphereVertexData[i].position.x, sphereVertexData[i].position.y, sphereVertexData[i].position.z);
	}

	sphereVertexBufferView.SizeInBytes = sizeof(VertexData) * uniqueVertexCount;
	sphereIndexBufferView.SizeInBytes = sizeof(uint32_t) * kVertexCount;
	//------------------------------------------------------Model------------------------------------------------------//


	//------------------------------------------------------Material------------------------------------------------------
	// マテリアル用のリソースを作る。--------------------------------------//
	ComPtr<ID3D12Resource> materialResource = dx12->MakeBufferResource(sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// アドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 赤
	materialData[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// ライティングを有効にする
	materialData[0].enableLighting = true;
	// uvTransformを初期化
	materialData[0].uvTransform = Mat4x4::MakeIdentity();

	Transform uvTransformModel{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f},
	};
	//------------------------------------------------------Material------------------------------------------------------//

	// WVP用のCBufferリソースを作る。----------------------------------------------//
	ComPtr<ID3D12Resource> modelWvpResource = dx12->MakeBufferResource(sizeof(TransformationMatrix));
	// WVPにデータを書き込む
	TransformationMatrix* modelTrasformationMatrixData = nullptr;
	// アドレスを取得
	modelWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&modelTrasformationMatrixData));
	// 単位行列を書き込んでいく
	modelTrasformationMatrixData->WVP = Mat4x4::MakeIdentity();
	modelTrasformationMatrixData->world = Mat4x4::MakeIdentity();

	Transform modelTransform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Transform cameraTransform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };

	ComPtr<ID3D12Resource> sphereWvpResource = dx12->MakeBufferResource(sizeof(TransformationMatrix));

	// WVPにデータを書き込む
	TransformationMatrix* sphereTrasformationMatrixData = nullptr;
	// アドレスを取得
	sphereWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereTrasformationMatrixData));

	// 単位行列を書き込んでいく
	sphereTrasformationMatrixData->WVP = Mat4x4::MakeIdentity();
	sphereTrasformationMatrixData->world = Mat4x4::MakeIdentity();

	Transform sphereTransform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	// ---------------------------------------------------Texture---------------------------------------------------
	// uvCheckerの読み込み
	DirectX::ScratchImage mipImages = dx12->LoadTexture("resources/uvChecker.png");
	const DirectX::TexMetadata& metaData = mipImages.GetMetadata();
	// Texture用のリソースを作成
	ComPtr<ID3D12Resource> textureResource = dx12->MakeTextureResource(metaData);
	// Textureのデータを転送
	ComPtr<ID3D12Resource> intermediateResource = dx12->UploadTextureData(textureResource.Get(), mipImages);

	// uvChecker用のSRVを作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metaData.format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = UINT(metaData.mipLevels);

	// uvCheckerのSRVを作成するDescriptorの位置を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = dx12->GetSRVCpuDescriptorHandle(1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = dx12->GetSRVGpuDescriptorHandle(1);

	// uvCheckerのSRVを作成
	dx12->GetDevice()->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	//-------------------------------------------------------------------------------------------------------//

	// planeのTextureの読み込み
	DirectX::ScratchImage planeMipImages = dx12->LoadTexture(planeData.material.texturePath);
	const DirectX::TexMetadata& metaData2 = planeMipImages.GetMetadata();
	// Texture用のリソースを作成
	ComPtr<ID3D12Resource> textureResource2 = dx12->MakeTextureResource(metaData2);
	// Textureのデータを転送
	ComPtr<ID3D12Resource> intermediateResource2 = dx12->UploadTextureData(textureResource2.Get(), planeMipImages);

	// SRVを作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metaData2.format;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.Texture2D.MipLevels = UINT(metaData2.mipLevels);

	// SRVを作成するDescriptorの位置を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = dx12->GetSRVCpuDescriptorHandle(2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = dx12->GetSRVGpuDescriptorHandle(2);

	// SRVを作成
	dx12->GetDevice()->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

	//-------------------------------------------------------------------------------------------------------//

	// teapotのTextureの読み込み
	DirectX::ScratchImage teapotMipImages = dx12->LoadTexture(teapotData.material.texturePath);
	const DirectX::TexMetadata& metaData3 = teapotMipImages.GetMetadata();
	// Texture用のリソースを作成
	ComPtr<ID3D12Resource> textureResource3 = dx12->MakeTextureResource(metaData3);
	// Textureのデータを転送
	ComPtr<ID3D12Resource> intermediateResource3 = dx12->UploadTextureData(textureResource3.Get(), teapotMipImages);

	// SRVを作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc3{};
	srvDesc3.Format = metaData3.format;
	srvDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc3.Texture2D.MipLevels = UINT(metaData3.mipLevels);

	// SRVを作成するDescriptorの位置を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU3 = dx12->GetSRVCpuDescriptorHandle(3);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 = dx12->GetSRVGpuDescriptorHandle(3);

	// SRVを作成
	dx12->GetDevice()->CreateShaderResourceView(textureResource3.Get(), &srvDesc3, textureSrvHandleCPU3);

	//-------------------------------------------------------------------------------------------------------//
	// mutiMaterialのTextureの読み込み
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> textureSrvHandleCPUs;
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> textureSrvHandleGPUs;

	for (int i = 0; i < multiMaterialModelDatas.size(); i++) {
		DirectX::ScratchImage multiMateMipImages = dx12->LoadTexture(multiMaterialModelDatas[i].material.texturePath);
		const DirectX::TexMetadata& multiMateMetaData = multiMateMipImages.GetMetadata();
		// Texture用のリソースを作成
		ComPtr<ID3D12Resource> multiMateTextureResource = dx12->MakeTextureResource(multiMateMetaData);
		// Textureのデータを転送
		ComPtr<ID3D12Resource> multiMateIntermediateResource = dx12->UploadTextureData(multiMateTextureResource.Get(), multiMateMipImages);

		// SRVを作成
		D3D12_SHADER_RESOURCE_VIEW_DESC multiMateSrvDesc{};
		multiMateSrvDesc.Format = multiMateMetaData.format;
		multiMateSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
		multiMateSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		multiMateSrvDesc.Texture2D.MipLevels = UINT(multiMateMetaData.mipLevels);

		// SRVを作成するDescriptorの位置を決める
		D3D12_CPU_DESCRIPTOR_HANDLE multiMateTextureSrvHandleCPU = dx12->GetSRVCpuDescriptorHandle(4 + i);
		D3D12_GPU_DESCRIPTOR_HANDLE multiMateTextureSrvHandleGPU = dx12->GetSRVGpuDescriptorHandle(4 + i);

		// SRVを作成
		dx12->GetDevice()->CreateShaderResourceView(multiMateTextureResource.Get(), &multiMateSrvDesc, multiMateTextureSrvHandleCPU);

		textureSrvHandleCPUs.push_back(multiMateTextureSrvHandleCPU);
		textureSrvHandleGPUs.push_back(multiMateTextureSrvHandleGPU);
	}

	// ---------------------------------------------------Texture---------------------------------------------------//


	//------------------------------------------------------Sprite------------------------------------------------------
	// textureの読み込み
	TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
	TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");
	uint32_t spriteNum = 5;
	std::vector<Sprite*> sprites;

	for (uint32_t i = 0; i < spriteNum; i++) {
		Sprite* sprite = new Sprite();
		if (i % 2 == 0)
			sprite->Initialize(spriteBasic, "resources/monsterBall.png");
		else
			sprite->Initialize(spriteBasic, "resources/checkerBoard.png");
		sprite->SetPos(Vector2(i * 150.0f, 0.0f));
		sprite->SetSize(Vector2(100.0f, 100.0f));
		sprites.push_back(sprite);
	}

	//Transform uvTransformSprite{
	//	{1.0f, 1.0f, 1.0f},
	//	{0.0f, 0.0f, 0.0f},
	//	{0.0f, 0.0f, 0.0f},
	//};

	//------------------------------------------------------Sprite------------------------------------------------------//

	//-------------------------------------------------------Light-------------------------------------------------------
	// 平行光源のリソースを作成
	ComPtr<ID3D12Resource> lightResource = dx12->MakeBufferResource(sizeof(DirectionalLight));

	// ライトにデータを書き込む
	DirectionalLight* lightData = nullptr;
	// Map
	lightResource->Map(0, nullptr, reinterpret_cast<void**>(&lightData));

	// ライトの方向
	lightData[0].direction = Vector3(0.0f, -1.0f, 0.0f);
	// ライトの色
	lightData[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// ライトのタイプ 0:Lambert 1:Half-Lambert
	lightData[0].lightType = 0;
	// 輝度
	lightData[0].intensity = 1.0f;
	//-------------------------------------------------------Light-------------------------------------------------------//



	//---------------------------------------------------GAMELOOP-----------------------------------------------------//

	// ウィンドウが閉じられるまでループ
	while (true)
	{
		// ウィンドウメッセージの取得
		if (winApp->ProcessMessage()) {
			// ウィンドウが閉じられたらループを抜ける
			break;
		}

		//-------------imguiの初期化-------------//
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		//-------------imguiの初期化-------------//

		/// <summary>
		/// 更新処理
		/// </summary>

		// 入力情報の更新
		input->Update();

		// modelの座標変換
		Matrix4x4 worldMatrix = Mat4x4::MakeAffine(modelTransform.scale, modelTransform.rotate, modelTransform.translate);
		Matrix4x4 cameraMatrix = Mat4x4::MakeAffine(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
		Matrix4x4 viewMatrix = Mat4x4::Inverse(cameraMatrix);
		Matrix4x4 projectionMatrix = Mat4x4::MakePerspective(0.45f, static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight), 0.1f, 100.0f);
		Matrix4x4 wvpMatrix = Mat4x4::Multiply(worldMatrix, Mat4x4::Multiply(viewMatrix, projectionMatrix));

		// modelのWVPにデータを書き込む
		modelTrasformationMatrixData->WVP = wvpMatrix;
		modelTrasformationMatrixData->world = worldMatrix;

		// Sphereの座標変換
		Matrix4x4 worldMatrixSphere = Mat4x4::MakeAffine(sphereTransform.scale, sphereTransform.rotate, sphereTransform.translate);
		Matrix4x4 wvpMatrixSphere = Mat4x4::Multiply(worldMatrixSphere, Mat4x4::Multiply(viewMatrix, projectionMatrix));

		sphereTrasformationMatrixData->WVP = wvpMatrixSphere;
		sphereTrasformationMatrixData->world = worldMatrixSphere;

		// modelのuvTransform
		Matrix4x4 uvTransformMatrixModel = Mat4x4::MakeAffine(uvTransformModel.scale, uvTransformModel.rotate, uvTransformModel.translate);
		materialData[0].uvTransform = uvTransformMatrixModel;

		// Spriteの更新
		for (uint32_t i = 0; i < spriteNum; i++) {
			sprites[i]->Update();
		}

		//worldMatrixSprite = Mat4x4::MakeAffine(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
		//viewMatrixSprite = Mat4x4::Inverse(cameraMatrix);
		//wvpMatrixSprite = Mat4x4::Multiply(worldMatrixSprite, Mat4x4::Multiply(viewMatrixSprite, projectionMatrixSprite));

		//TrasformationMatrixDataSprite->WVP = wvpMatrixSprite;
		//TrasformationMatrixDataSprite->world = worldMatrixSprite;

		// SpriteのuvTransform
		//Matrix4x4 uvTransformMatrix = Mat4x4::MakeAffine(uvTransformSprite.scale, uvTransformSprite.rotate, uvTransformSprite.translate);
		//materialDataSprite[0].uvTransform = uvTransformMatrix;

		if (input->PushKey(DIK_W)) {
			modelTransform.translate.y += 0.1f;
		}
		if (input->PushKey(DIK_S)) {
			modelTransform.translate.y -= 0.1f;
		}
		if (input->PushKey(DIK_A)) {
			modelTransform.translate.x -= 0.1f;
		}
		if (input->PushKey(DIK_D)) {
			modelTransform.translate.x += 0.1f;
		}

		/// <summary>
		/// 描画処理
		/// </summary>

		// 描画前の処理
		dx12->BeginDraw();

		//-------------------ImGui-------------------//
		ImGui::Begin("Option");

		if (ImGui::BeginTabBar("Option"))
		{
			if (ImGui::BeginTabItem("obj Model"))
			{
				const char* ModelType_items[] = { "Plane", "teapot", "bunny", "MultiMesh", "MultiMaterial", "Suzanne" };
				static int ModelType_item_current = 0;
				ImGui::Combo("ModelType", &ModelType_item_current, ModelType_items, IM_ARRAYSIZE(ModelType_items));
				if (ImGui::Button("Load"))
				{
					if (ModelType_item_current == 0)
					{
						modelType = Plane;
					} else if (ModelType_item_current == 1)
					{
						modelType = Teapot;
					} else if (ModelType_item_current == 2)
					{
						modelType = Bunny;
					} else if (ModelType_item_current == 3)
					{
						modelType = MultiMesh;
					} else if (ModelType_item_current == 4)
					{
						modelType = MultiMaterial;
					} else if (ModelType_item_current == 5)
					{
						modelType = Suzanne;
					}
				}

				ImGui::Separator();

				ImGui::DragFloat3("Scale", &modelTransform.scale.x, 0.1f, 0.0f, 50.0f);
				ImGui::DragFloat3("Rotate", &modelTransform.rotate.x, 0.1f, 0.0f, 6.28f);
				ImGui::DragFloat3("Translate", &modelTransform.translate.x, 0.1f, -50.0f, 50.0f);
				ImGui::ColorEdit4("Color", &materialData[0].color.x);

				ImGui::Separator();

				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Sphere"))
			{
				ImGui::DragFloat3("Scale", &sphereTransform.scale.x, 0.1f, 0.0f, 50.0f);
				ImGui::DragFloat3("Rotate", &sphereTransform.rotate.x, 0.1f, 0.0f, 6.28f);
				ImGui::DragFloat3("Translate", &sphereTransform.translate.x, 0.1f, -50.0f, 50.0f);
				ImGui::EndTabItem();
			}
			//if (ImGui::BeginTabItem("Sprite"))
			//{
			//	ImGui::DragFloat3("Scale", &transformSprite.scale.x, 0.1f, 0.0f, 50.0f);
			//	ImGui::DragFloat3("Rotate", &transformSprite.rotate.x, 0.1f, 0.0f, 6.28f);
			//	ImGui::DragFloat3("Translate", &transformSprite.translate.x, 1.0f, -1000.0f, 1000.0f);
			//	ImGui::ColorEdit4("Color", &colorSprite.x);
			//	sprite->SetTransform(transformSprite);
			//	sprite->SetColor(colorSprite);
			//	ImGui::EndTabItem();
			//}
			if (ImGui::BeginTabItem("Light"))
			{
				ImGui::Checkbox("EnableLighting", &materialData[0].enableLighting);
				ImGui::Separator();
				ImGui::DragFloat3("Direction", &lightData[0].direction.x, 0.1f, -1.0f, 1.0f);
				ImGui::ColorEdit4("Color", &lightData[0].color.x);
				ImGui::DragFloat("Intensity", &lightData[0].intensity, 0.1f, 0.0f, 10.0f);
				ImGui::Separator();
				const char* LightType_items[] = { "Lambert", "Half-Lambert" };
				static int LightType_item_current = 0;
				ImGui::Combo("LightType", &LightType_item_current, LightType_items, IM_ARRAYSIZE(LightType_items));
				lightData[0].lightType = LightType_item_current;
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Sound"))
			{
				if (ImGui::Button("Play"))
				{
					SoundPlay(xAudio2.Get(), soundData);
				}
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();

		ImGui::Begin("UVTransform");
		if (ImGui::BeginTabBar("Option"))
		{
			//if (ImGui::BeginTabItem("Sprite"))
			//{
			//	ImGui::DragFloat2("Scale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
			//	ImGui::DragFloat2("Translate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
			//	ImGui::SliderAngle("Rotate", &uvTransformSprite.rotate.z);
			//	ImGui::EndTabItem();
			//}
			if (ImGui::BeginTabItem("Model"))
			{
				ImGui::DragFloat2("Scale", &uvTransformModel.scale.x, 0.01f, -10.0f, 10.0f);
				ImGui::DragFloat2("Translate", &uvTransformModel.translate.x, 0.01f, -10.0f, 10.0f);
				ImGui::SliderAngle("Rotate", &uvTransformModel.rotate.z);
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();

		// ImGuiの内部コマンドを生成。描画処理の前に行う
		ImGui::Render();

		//-------------------ImGui-------------------//

		// 共通描画設定
		spriteBasic->SetCommonRenderSetting();

		//-----------Modelの描画-----------//
		// マテリアルの設定。色を変える
		dx12->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress()); // マテリアルCBufferの場所を設定

		// WVPのcBufferの設定
		dx12->GetCommandList()->SetGraphicsRootConstantBufferView(1, modelWvpResource->GetGPUVirtualAddress()); // WVPのCBufferの場所を設定

		// Lightの設定
		dx12->GetCommandList()->SetGraphicsRootConstantBufferView(3, lightResource->GetGPUVirtualAddress());

		if (modelType == Plane)
		{
			// Textureの設定
			//dx12->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU2);

			// 頂点バッファの設定
			dx12->GetCommandList()->IASetVertexBuffers(0, 1, &planeVertexBufferView);

			// 描画
			dx12->GetCommandList()->DrawInstanced(UINT(planeData.vertices.size()), 1, 0, 0);
		} else if (modelType == Teapot)
		{
			// Textureの設定
			//dx12->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU3);

			// 頂点バッファの設定
			dx12->GetCommandList()->IASetVertexBuffers(0, 1, &teapotVertexBufferView);

			// 描画
			dx12->GetCommandList()->DrawInstanced(UINT(teapotData.vertices.size()), 1, 0, 0);
		} else if (modelType == Bunny)
		{
			// Textureの設定
			//dx12->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

			// 頂点バッファの設定
			dx12->GetCommandList()->IASetVertexBuffers(0, 1, &bunnyVertexBufferView);

			// 描画
			dx12->GetCommandList()->DrawInstanced(UINT(bunnyData.vertices.size()), 1, 0, 0);
		} else if (modelType == MultiMesh) {
			for (int i = 0; i < multiMeshModelDatas.size(); i++)
			{
				// Textureの設定
				//dx12->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

				// 頂点バッファの設定
				dx12->GetCommandList()->IASetVertexBuffers(0, 1, &multiMeshVertexBufferViews[i]);

				// 描画
				dx12->GetCommandList()->DrawInstanced(UINT(multiMeshModelDatas[i].vertices.size()), 1, 0, 0);
			}
		} else if (modelType == MultiMaterial) {
			for (int i = 0; i < multiMaterialModelDatas.size(); i++)
			{
				// Textureの設定
				//dx12->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPUs[i]);

				// 頂点バッファの設定
				dx12->GetCommandList()->IASetVertexBuffers(0, 1, &multiMaterialVertexBufferViews[i]);

				// 描画
				dx12->GetCommandList()->DrawInstanced(UINT(multiMaterialModelDatas[i].vertices.size()), 1, 0, 0);
			}
		}

		//-----------Modelの描画-----------//

		//-----------Sphereの描画-----------//
		// WVPのcBufferの設定
		dx12->GetCommandList()->SetGraphicsRootConstantBufferView(1, sphereWvpResource->GetGPUVirtualAddress()); // WVPのCBufferの場所を設定

		// Textureの設定
		//dx12->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

		// 頂点バッファの設定
		dx12->GetCommandList()->IASetVertexBuffers(0, 1, &sphereVertexBufferView);

		// インデックスバッファの設定
		dx12->GetCommandList()->IASetIndexBuffer(&sphereIndexBufferView);

		// 描画
		dx12->GetCommandList()->DrawIndexedInstanced(kVertexCount, 1, 0, 0, 0);
		//-----------Sphereの描画-----------//


		//-----------Spriteの描画-----------//

		// Textureの設定
		//dx12->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

		for (uint32_t i = 0; i < spriteNum; i++)
		{
			// Spriteの描画
			sprites[i]->Draw();
		}

		//-----------Spriteの描画-----------//

		//-----------Suzanneの描画-----------//
		if (modelType == Suzanne)
		{
			// ルートシグネチャの設定
			dx12->GetCommandList()->SetGraphicsRootSignature(rootSignatureNoTex.Get());

			// PSOの設定
			dx12->GetCommandList()->SetPipelineState(graphicsPipelineStateNoTex.Get());

			// マテリアルの設定。色を変える
			dx12->GetCommandList()->SetGraphicsRootConstantBufferView(1, materialResource->GetGPUVirtualAddress()); // マテリアルCBufferの場所を設定

			// WVPのcBufferの設定
			dx12->GetCommandList()->SetGraphicsRootConstantBufferView(0, modelWvpResource->GetGPUVirtualAddress()); // WVPのCBufferの場所を設定

			// Lightの設定
			dx12->GetCommandList()->SetGraphicsRootConstantBufferView(2, lightResource->GetGPUVirtualAddress()); // LightのCBufferの場所を設定

			// 頂点バッファの設定
			dx12->GetCommandList()->IASetVertexBuffers(0, 1, &suzanneVertexBufferView);

			// 描画
			dx12->GetCommandList()->DrawInstanced(UINT(suzanneData.vertices.size()), 1, 0, 0);
		}
		//-----------Suzanneの描画-----------//


		// commandListにimguiの描画コマンドを積む。描画処理の後、RTVからPRESENT Stateに戻す前に行う
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx12->GetCommandList());

		// 描画後の処理
		dx12->EndDraw();

	}

	//-------------------------------------------------GAMELOOP-----------------------------------------------------/

	// リソースの解放
	//vertexShaderBlob->Release();
	//pixelShaderBlob->Release();
	//vertexShaderNoTexBlob->Release();
	//pixelShaderNoTexBlob->Release();
	//rootSignature->Release();
	//rootSignatureNoTex->Release();

	TextureManager::GetInstance()->Finalize();

	dx12->Finalize();

	// XAudio2の解放
	xAudio2.Reset();
	SoundUnload(&soundData);

	// pointerの解放


	// pointerの解放
	delete input;
	delete dx12;
	delete spriteBasic;

	for (uint32_t i = 0; i < spriteNum; i++)
	{
		delete sprites[i];
	}

#ifdef _DEBUG
	//debugController->Release();
#endif 

	winApp->Finalize();

	// ウィンドウクラスの解放
	delete winApp;



	return 0;
}


// 関数の定義-------------------------------------------------------------------------------------------------------------------

ModelData LoadObjFile(const std::string& directoryPath, const std::string& fileName)
{
	ModelData modelData;
	VertexData triangleVertices[3];
	std::vector<Vector4> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;
	std::string line;

	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);

		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);

		} else if (identifier == "f") {

			for (int32_t facevertex = 0; facevertex < 3; facevertex++) {
				std::string vertexDefiniton;
				s >> vertexDefiniton;

				std::istringstream v(vertexDefiniton);
				uint32_t elementIndices[3];

				for (int32_t element = 0; element < 3; element++) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				position.z *= -1.0f;
				normal.z *= -1.0f;
				texcoord.y = 1.0f - texcoord.y;

				triangleVertices[facevertex] = { position, texcoord, normal };

			}

			// 三角形の頂点データを追加
			modelData.vertices.push_back(triangleVertices[2]);
			modelData.vertices.push_back(triangleVertices[1]);
			modelData.vertices.push_back(triangleVertices[0]);

		} else if (identifier == "mtllib") {
			std::string mtlFileName;
			s >> mtlFileName;
			modelData.material = LoadMtlFile(directoryPath, mtlFileName);
		}
	}

	return modelData;
}

MaterialData LoadMtlFile(const std::string& directoryPath, const std::string& fileName)
{
	MaterialData materialData;
	std::string line;

	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "map_Kd") {
			std::string textureFileName;
			s >> textureFileName;
			materialData.texturePath = directoryPath + "/" + textureFileName;
		}
	}

	return materialData;
}

std::vector<ModelData> LoadMutiMeshObjFile(const std::string& directoryPath, const std::string& fileName)
{
	std::vector<ModelData> modelDatas;
	ModelData modelData;
	VertexData triangleVertices[3];
	std::vector<Vector4> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;
	std::string line;

	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);

		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);

		} else if (identifier == "f") {

			for (int32_t facevertex = 0; facevertex < 3; facevertex++) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];

				for (int32_t element = 0; element < 3; element++) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				position.z *= -1.0f;
				normal.z *= -1.0f;
				texcoord.y = 1.0f - texcoord.y;

				triangleVertices[facevertex] = { position, texcoord, normal };

			}

			// Add the triangle vertices in reverse order
			modelData.vertices.push_back(triangleVertices[2]);
			modelData.vertices.push_back(triangleVertices[1]);
			modelData.vertices.push_back(triangleVertices[0]);

		} else if (identifier == "o" || identifier == "g") {
			if (!modelData.vertices.empty()) {
				modelDatas.push_back(modelData);
				modelData = ModelData();
			}
		} else if (identifier == "mtllib") {
			std::string mtlFileName;
			s >> mtlFileName;
			modelData.material = LoadMtlFile(directoryPath, mtlFileName);
		}
	}

	if (!modelData.vertices.empty()) {
		modelDatas.push_back(modelData);
	}

	return modelDatas;
}

std::vector<ModelData> LoadMutiMaterialFile(const std::string& directoryPath, const std::string& fileName)
{
	std::vector<ModelData> modelDatas;
	ModelData modelData;
	VertexData triangleVertices[3];
	std::vector<Vector4> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;
	std::unordered_map<std::string, MaterialData> materials;
	std::string currentMaterial;
	std::string line;

	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);

		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);

		} else if (identifier == "f") {

			for (int32_t facevertex = 0; facevertex < 3; facevertex++) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];

				for (int32_t element = 0; element < 3; element++) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				position.z *= -1.0f;
				normal.z *= -1.0f;
				texcoord.y = 1.0f - texcoord.y;

				triangleVertices[facevertex] = { position, texcoord, normal };
			}

			modelData.vertices.push_back(triangleVertices[2]);
			modelData.vertices.push_back(triangleVertices[1]);
			modelData.vertices.push_back(triangleVertices[0]);

		} else if (identifier == "o" || identifier == "g") {
			if (!modelData.vertices.empty()) {
				modelDatas.push_back(modelData);
				modelData = ModelData();
			}
		} else if (identifier == "usemtl") {
			s >> currentMaterial;
			if (!modelData.vertices.empty()) {
				modelDatas.push_back(modelData);
				modelData = ModelData();
			}
			modelData.material = materials[currentMaterial];
		} else if (identifier == "mtllib") {
			std::string mtlFileName;
			s >> mtlFileName;
			materials = LoadMutiMaterialMtlFile(directoryPath, mtlFileName);
		}
	}

	if (!modelData.vertices.empty()) {
		modelDatas.push_back(modelData);
	}

	return modelDatas;
}

std::unordered_map<std::string, MaterialData> LoadMutiMaterialMtlFile(const std::string& directoryPath, const std::string& fileName)
{
	std::unordered_map<std::string, MaterialData> materials;
	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	std::string line, currentMaterialName;

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "newmtl") {
			s >> currentMaterialName;
			materials[currentMaterialName] = MaterialData(); // マテリアルを初期化
		} else if (identifier == "map_Kd") {
			std::string textureFileName;
			s >> textureFileName;
			materials[currentMaterialName].texturePath = directoryPath + "/" + textureFileName;
		}
	}

	return materials;
}

ModelDataNoTex LoadObjFileNoTex(const std::string& directoryPath, const std::string& fileName) {
	ModelDataNoTex modelData;
	VertexDataNoTex triangleVertices[3];
	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::string line;

	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);

		} else if (identifier == "f") {

			for (int32_t facevertex = 0; facevertex < 3; facevertex++) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				// 文字列を二つのスラッシュ "//" で分割する
				size_t firstSlash = vertexDefinition.find("//");
				size_t secondSlash = vertexDefinition.find("//", firstSlash + 2);

				// 頂点位置と法線のインデックスを抽出する
				uint32_t positionIndex = std::stoi(vertexDefinition.substr(0, firstSlash));
				uint32_t normalIndex = std::stoi(vertexDefinition.substr(firstSlash + 2, secondSlash - (firstSlash + 2)));

				Vector4 position = positions[positionIndex - 1];
				Vector3 normal = normals[normalIndex - 1];

				// 座標系変換を処理する
				position.z *= -1.0f;
				normal.z *= -1.0f;

				triangleVertices[facevertex] = { position, normal };
			}

			// 三角形の頂点データを追加する
			modelData.vertices.push_back(triangleVertices[2]);
			modelData.vertices.push_back(triangleVertices[1]);
			modelData.vertices.push_back(triangleVertices[0]);
		}
	}

	return modelData;
}

SoundData LoadWaveFile(const char* filename)
{
	//HRESULT result;

	std::ifstream file;
	// バイナリモードで開く
	file.open(filename, std::ios::binary);
	assert(file.is_open());

	// wavファイルのヘッダーを読み込む
	RiffHeader riff;
	file.read(reinterpret_cast<char*>(&riff), sizeof(riff));

	if (strncmp(riff.chunk.id, "RIFF", 4) != 0 || strncmp(riff.type, "WAVE", 4) != 0)
	{
		assert(false);
	}


	FormatChunk format = {};
	file.read(reinterpret_cast<char*>(&format), sizeof(ChunkHeader));

	if (strncmp(format.chunk.id, "fmt ", 4) != 0)
	{
		assert(false);
	}
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read(reinterpret_cast<char*>(&format.fmt), format.chunk.size);


	ChunkHeader data;
	file.read(reinterpret_cast<char*>(&data), sizeof(data));

	if (strncmp(data.id, "JUNK ", 4) == 0)
	{
		file.seekg(data.size, std::ios::cur);
		file.read(reinterpret_cast<char*>(&data), sizeof(data));
	}

	if (strncmp(data.id, "data ", 4) != 0)
	{
		assert(false);
	}

	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	file.close();

	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}

void SoundUnload(SoundData* soundData) {
	delete[] soundData->pBuffer;
	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

void SoundPlay(IXAudio2* xAudio2, const SoundData& soundData) {
	HRESULT result;

	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	XAUDIO2_BUFFER buffer = {};
	buffer.pAudioData = soundData.pBuffer;
	buffer.AudioBytes = soundData.bufferSize;
	buffer.Flags = XAUDIO2_END_OF_STREAM;

	result = pSourceVoice->SubmitSourceBuffer(&buffer);
	result = pSourceVoice->Start();
}