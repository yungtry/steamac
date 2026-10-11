// d3d12bench: D3D12 feature probe + the same load as d3d11bench, for comparing D3D12 implementations.
// Usage: d3d12bench.exe [--draws N] [--overdraw K] [--seconds S] [--width W] [--height H] [--label L]
//                       [--probe-only 1] [--indirect 1] [--out FILE]
// Prints "PROBE name=value" lines (CheckFeatureSupport results and functional checks), then
// "RESULT ..." like d3d11bench (first second excluded). Shaders are SM 5.1 DXBC compiled at runtime.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static int g_draws = 2000, g_overdraw = 4, g_seconds = 15, g_w = 1280, g_h = 800, g_probe_only = 0, g_indirect = 0;
static const char *g_label = "run";
static bool g_quit;
static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_CLOSE) { g_quit = true; return 0; }
  return DefWindowProcA(h, m, w, l);
}
#define PROBE(name, fmt, ...) printf("PROBE %s=" fmt "\n", name, __VA_ARGS__)
#define CHECKHR(x) do { HRESULT _h = (x); if (FAILED(_h)) { printf("FAIL %s hr=0x%08lx\n", #x, (unsigned long)_h); fflush(stdout); exit(2); } } while (0)

static const char *kShader = R"(
cbuffer RC : register(b0) { float4 xform; float4 tint; };
Texture2D tex0 : register(t0);
SamplerState smp : register(s0);
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
VSOut vs_main(uint id : SV_VertexID) {
  float2 c[6] = { float2(-1,-1), float2(1,-1), float2(1,1), float2(-1,-1), float2(1,1), float2(-1,1) };
  float2 p = c[id];
  float s = sin(xform.w), co = cos(xform.w);
  float2 r = float2(p.x * co - p.y * s, p.x * s + p.y * co) * xform.z + xform.xy;
  VSOut o; o.pos = float4(r, 0.5, 1); o.uv = p * 0.5 + 0.5; return o; }
float4 ps_main(VSOut i) : SV_Target { return tex0.Sample(smp, i.uv) * tint; }
VSOut fs_vs(uint id : SV_VertexID) {
  float2 p = float2((id << 1) & 2, id & 2); VSOut o; o.uv = p; o.pos = float4(p * float2(2, -2) + float2(-1, 1), 0.5, 1); return o; }
float4 fs_ps(VSOut i) : SV_Target {
  float3 c = 0; float2 uv = i.uv;
  [unroll] for (int k = 0; k < 24; k++) { uv = frac(uv * 1.37 + float2(0.11, 0.07) * k); c += tex0.Sample(smp, uv).rgb * (1.0 / 24); }
  return float4(c * tint.rgb, 0.08); }
)";

static ID3DBlob *compile(const char *entry, const char *target) {
  ID3DBlob *code = nullptr, *err = nullptr;
  if (FAILED(D3DCompile(kShader, strlen(kShader), "bench", nullptr, nullptr, entry, target, 0, 0, &code, &err))) {
    printf("FAIL compile %s: %s\n", entry, err ? (const char *)err->GetBufferPointer() : "?");
    exit(2);
  }
  return code;
}

int main(int argc, char **argv) {
  for (int i = 1; i + 1 < argc; i += 2) {
    if (!strcmp(argv[i], "--draws")) g_draws = atoi(argv[i + 1]);
    else if (!strcmp(argv[i], "--overdraw")) g_overdraw = atoi(argv[i + 1]);
    else if (!strcmp(argv[i], "--seconds")) g_seconds = atoi(argv[i + 1]);
    else if (!strcmp(argv[i], "--width")) g_w = atoi(argv[i + 1]);
    else if (!strcmp(argv[i], "--height")) g_h = atoi(argv[i + 1]);
    else if (!strcmp(argv[i], "--label")) g_label = argv[i + 1];
    else if (!strcmp(argv[i], "--probe-only")) g_probe_only = atoi(argv[i + 1]);
    else if (!strcmp(argv[i], "--indirect")) g_indirect = atoi(argv[i + 1]);
    else if (!strcmp(argv[i], "--out")) freopen(argv[i + 1], "w", stdout); /* stdout is lost under Steam's launch chain */
  }
  IDXGIFactory4 *factory; CHECKHR(CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void **)&factory));
  ID3D12Device *dev = nullptr;
  HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void **)&dev);
  PROBE("create_device_fl11_0", "0x%08lx", (unsigned long)hr);
  if (FAILED(hr)) return 2;
  hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr);
  PROBE("create_device_fl12_0", "0x%08lx", (unsigned long)hr);

  // ---- feature queries
  D3D_FEATURE_LEVEL req[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_2};
  D3D12_FEATURE_DATA_FEATURE_LEVELS fl = {5, req, D3D_FEATURE_LEVEL_11_0};
  hr = dev->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &fl, sizeof fl);
  PROBE("max_feature_level", "0x%x hr=0x%08lx", fl.MaxSupportedFeatureLevel, (unsigned long)hr);
  D3D12_FEATURE_DATA_SHADER_MODEL sm = {(D3D_SHADER_MODEL)0x66};
  hr = dev->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &sm, sizeof sm);
  PROBE("highest_shader_model", "0x%x hr=0x%08lx", sm.HighestShaderModel, (unsigned long)hr);
  D3D12_FEATURE_DATA_ROOT_SIGNATURE rs = {D3D_ROOT_SIGNATURE_VERSION_1_1};
  hr = dev->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rs, sizeof rs);
  PROBE("root_signature", "0x%x hr=0x%08lx", rs.HighestVersion, (unsigned long)hr);
  D3D12_FEATURE_DATA_D3D12_OPTIONS o0 = {};
  hr = dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &o0, sizeof o0);
  PROBE("options", "binding_tier=%d tiled_tier=%d heap_tier=%d conservative=%d rov=%d hr=0x%08lx", o0.ResourceBindingTier,
        o0.TiledResourcesTier, o0.ResourceHeapTier, o0.ConservativeRasterizationTier, o0.ROVsSupported, (unsigned long)hr);
  D3D12_FEATURE_DATA_D3D12_OPTIONS1 o1 = {};
  hr = dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &o1, sizeof o1);
  PROBE("wave_ops", "%d lanes=%u..%u hr=0x%08lx", o1.WaveOps, o1.WaveLaneCountMin, o1.WaveLaneCountMax, (unsigned long)hr);
  D3D12_FEATURE_DATA_D3D12_OPTIONS5 o5 = {};
  hr = dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &o5, sizeof o5);
  PROBE("raytracing_tier", "%d hr=0x%08lx", o5.RaytracingTier, (unsigned long)hr);
  D3D12_FEATURE_DATA_D3D12_OPTIONS7 o7 = {};
  hr = dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &o7, sizeof o7);
  PROBE("mesh_shader_tier", "%d sampler_feedback=%d hr=0x%08lx", o7.MeshShaderTier, o7.SamplerFeedbackTier, (unsigned long)hr);

  // ---- functional checks
  {
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; rd.Width = 4096; rd.Height = 4096; rd.DepthOrArraySize = 1;
    rd.MipLevels = 1; rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
    ID3D12Resource *r = nullptr;
    hr = dev->CreateReservedResource(&rd, D3D12_RESOURCE_STATE_COMMON, nullptr, __uuidof(ID3D12Resource), (void **)&r);
    PROBE("reserved_resource_create", "0x%08lx", (unsigned long)hr);
    if (r) r->Release();
  }

  // ---- window + queue + swapchain
  WNDCLASSA wc = {}; wc.lpfnWndProc = wndproc; wc.hInstance = GetModuleHandleA(nullptr); wc.lpszClassName = "d3d12bench";
  RegisterClassA(&wc);
  RECT rc = {0, 0, g_w, g_h}; AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
  HWND hwnd = CreateWindowA("d3d12bench", "d3d12bench", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 40, 40, rc.right - rc.left,
                            rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr);
  D3D12_COMMAND_QUEUE_DESC qd = {D3D12_COMMAND_LIST_TYPE_DIRECT};
  ID3D12CommandQueue *queue; CHECKHR(dev->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), (void **)&queue));
  const UINT FRAMES = 3;
  DXGI_SWAP_CHAIN_DESC1 sd = {};
  sd.Width = g_w; sd.Height = g_h; sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.SampleDesc.Count = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = FRAMES; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  IDXGISwapChain1 *sc1; CHECKHR(factory->CreateSwapChainForHwnd(queue, hwnd, &sd, nullptr, nullptr, &sc1));
  IDXGISwapChain3 *sc; CHECKHR(sc1->QueryInterface(__uuidof(IDXGISwapChain3), (void **)&sc));

  D3D12_DESCRIPTOR_HEAP_DESC rh = {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, FRAMES};
  ID3D12DescriptorHeap *rtv_heap; CHECKHR(dev->CreateDescriptorHeap(&rh, __uuidof(ID3D12DescriptorHeap), (void **)&rtv_heap));
  UINT rtv_inc = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  ID3D12Resource *bb[FRAMES];
  for (UINT i = 0; i < FRAMES; i++) {
    CHECKHR(sc->GetBuffer(i, __uuidof(ID3D12Resource), (void **)&bb[i]));
    D3D12_CPU_DESCRIPTOR_HANDLE h = rtv_heap->GetCPUDescriptorHandleForHeapStart(); h.ptr += i * rtv_inc;
    dev->CreateRenderTargetView(bb[i], nullptr, h);
  }
  D3D12_DESCRIPTOR_HEAP_DESC sh = {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
  ID3D12DescriptorHeap *srv_heap;
  hr = dev->CreateDescriptorHeap(&sh, __uuidof(ID3D12DescriptorHeap), (void **)&srv_heap);
  PROBE("shader_visible_heap", "0x%08lx", (unsigned long)hr);
  if (FAILED(hr)) return 2;

  // root signature 1.1: 8 root constants (b0), SRV table (t0), static sampler (s0)
  D3D12_DESCRIPTOR_RANGE1 range = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 0};
  D3D12_ROOT_PARAMETER1 params[2] = {};
  params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; params[0].Constants = {0, 0, 8};
  params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; params[1].DescriptorTable = {1, &range};
  params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  D3D12_STATIC_SAMPLER_DESC ss = {}; ss.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  ss.AddressU = ss.AddressV = ss.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; ss.MaxLOD = D3D12_FLOAT32_MAX;
  ss.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  D3D12_VERSIONED_ROOT_SIGNATURE_DESC rsd = {D3D_ROOT_SIGNATURE_VERSION_1_1};
  rsd.Desc_1_1 = {2, params, 1, &ss, D3D12_ROOT_SIGNATURE_FLAG_NONE};
  ID3DBlob *rsb = nullptr, *rse = nullptr;
  hr = D3D12SerializeVersionedRootSignature(&rsd, &rsb, &rse);
  PROBE("serialize_root_signature_1_1", "0x%08lx", (unsigned long)hr);
  if (FAILED(hr)) return 2;
  ID3D12RootSignature *root;
  hr = dev->CreateRootSignature(0, rsb->GetBufferPointer(), rsb->GetBufferSize(), __uuidof(ID3D12RootSignature), (void **)&root);
  PROBE("create_root_signature_1_1", "0x%08lx", (unsigned long)hr);
  if (FAILED(hr)) return 2;

  ID3DBlob *vsb = compile("vs_main", "vs_5_1"), *psb = compile("ps_main", "ps_5_1");
  ID3DBlob *fvb = compile("fs_vs", "vs_5_1"), *fpb = compile("fs_ps", "ps_5_1");
  auto make_pso = [&](ID3DBlob *v, ID3DBlob *p, bool blend) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d = {};
    d.pRootSignature = root; d.VS = {v->GetBufferPointer(), v->GetBufferSize()}; d.PS = {p->GetBufferPointer(), p->GetBufferSize()};
    d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    if (blend) {
      auto &t = d.BlendState.RenderTarget[0];
      t.BlendEnable = TRUE; t.SrcBlend = D3D12_BLEND_SRC_ALPHA; t.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; t.BlendOp = D3D12_BLEND_OP_ADD;
      t.SrcBlendAlpha = D3D12_BLEND_ONE; t.DestBlendAlpha = D3D12_BLEND_ZERO; t.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    }
    d.SampleMask = 0xffffffff; d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE; d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1; d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1;
    ID3D12PipelineState *pso = nullptr;
    HRESULT h = dev->CreateGraphicsPipelineState(&d, __uuidof(ID3D12PipelineState), (void **)&pso);
    PROBE(blend ? "pso_fill" : "pso_draw", "0x%08lx", (unsigned long)h);
    if (FAILED(h)) exit(2);
    return pso;
  };
  ID3D12PipelineState *pso = make_pso(vsb, psb, false), *fpso = make_pso(fvb, fpb, true);

  // texture upload (256x256 checker) through an upload buffer
  const UINT TW = 256;
  D3D12_HEAP_PROPERTIES hp_default = {D3D12_HEAP_TYPE_DEFAULT}, hp_upload = {D3D12_HEAP_TYPE_UPLOAD};
  D3D12_RESOURCE_DESC td = {D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, TW, TW, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}};
  ID3D12Resource *tex;
  CHECKHR(dev->CreateCommittedResource(&hp_default, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource), (void **)&tex));
  D3D12_RESOURCE_DESC bd = {D3D12_RESOURCE_DIMENSION_BUFFER, 0, TW * TW * 4, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR};
  ID3D12Resource *up;
  CHECKHR(dev->CreateCommittedResource(&hp_upload, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void **)&up));
  unsigned *px; D3D12_RANGE nr = {0, 0}; CHECKHR(up->Map(0, &nr, (void **)&px));
  for (UINT y = 0; y < TW; y++) for (UINT x = 0; x < TW; x++) px[y * TW + x] = ((x ^ y) & 16) ? 0xffd0a040u : 0xff4060c0u;
  up->Unmap(0, nullptr);
  D3D12_SHADER_RESOURCE_VIEW_DESC srvd = {}; srvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; srvd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; srvd.Texture2D.MipLevels = 1;
  dev->CreateShaderResourceView(tex, &srvd, srv_heap->GetCPUDescriptorHandleForHeapStart());

  ID3D12CommandAllocator *alloc[FRAMES];
  for (UINT i = 0; i < FRAMES; i++) CHECKHR(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void **)&alloc[i]));
  ID3D12GraphicsCommandList *cl;
  CHECKHR(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc[0], nullptr, __uuidof(ID3D12GraphicsCommandList), (void **)&cl));
  D3D12_TEXTURE_COPY_LOCATION dst = {tex, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX}; dst.SubresourceIndex = 0;
  D3D12_TEXTURE_COPY_LOCATION src = {up, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
  src.PlacedFootprint = {0, {DXGI_FORMAT_R8G8B8A8_UNORM, TW, TW, 1, TW * 4}};
  cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
  D3D12_RESOURCE_BARRIER tb = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION};
  tb.Transition = {tex, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
  cl->ResourceBarrier(1, &tb);
  cl->Close();
  ID3D12CommandList *lists[] = {cl}; queue->ExecuteCommandLists(1, lists);
  ID3D12Fence *fence; CHECKHR(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&fence));
  HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
  UINT64 fv = 1; queue->Signal(fence, fv);
  fence->SetEventOnCompletion(fv, ev); WaitForSingleObject(ev, 5000);

  // ExecuteIndirect draw command signature (draw with 8 root constants per command)
  ID3D12CommandSignature *csig = nullptr; ID3D12Resource *argbuf = nullptr;
  {
    D3D12_INDIRECT_ARGUMENT_DESC a[2] = {};
    a[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT; a[0].Constant = {0, 0, 8};
    a[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    D3D12_COMMAND_SIGNATURE_DESC cs = {48, 2, a, 0};
    hr = dev->CreateCommandSignature(&cs, root, __uuidof(ID3D12CommandSignature), (void **)&csig);
    PROBE("command_signature_draw_constants", "0x%08lx", (unsigned long)hr);
    D3D12_RESOURCE_DESC ad = bd; ad.Width = 48ull * std::max(g_draws, 1);
    hr = dev->CreateCommittedResource(&hp_upload, D3D12_HEAP_FLAG_NONE, &ad, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void **)&argbuf);
  }
  if (g_probe_only) { printf("PROBE done=1\n"); return 0; }

  int grid = (int)ceil(sqrt((double)std::max(g_draws, 1)));
  if (argbuf) { // static per-draw arguments for the indirect mode
    unsigned char *a; argbuf->Map(0, &nr, (void **)&a);
    for (int d = 0; d < g_draws; d++) {
      float *f = (float *)(a + 48 * d);
      f[0] = (d % grid + 0.5f) / grid * 2 - 1; f[1] = (d / grid + 0.5f) / grid * 2 - 1; f[2] = 0.9f / grid; f[3] = d * 0.01f;
      f[4] = 0.5f + 0.5f * sinf(d * 0.1f); f[5] = 0.8f; f[6] = 1; f[7] = 1;
      UINT *u = (UINT *)(f + 8); u[0] = 6; u[1] = 1; u[2] = 0; u[3] = 0;
    }
    argbuf->Unmap(0, nullptr);
  }
  UINT64 frame_fence[FRAMES] = {};
  LARGE_INTEGER freq, t0, tn; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&t0);
  std::vector<double> ms; double prev = 0;
  for (unsigned frame = 0; !g_quit; frame++) {
    MSG msg; while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    QueryPerformanceCounter(&tn);
    double t = double(tn.QuadPart - t0.QuadPart) / freq.QuadPart;
    if (t > g_seconds) break;
    UINT bi = sc->GetCurrentBackBufferIndex();
    if (fence->GetCompletedValue() < frame_fence[bi]) { fence->SetEventOnCompletion(frame_fence[bi], ev); WaitForSingleObject(ev, 5000); }
    alloc[bi]->Reset(); cl->Reset(alloc[bi], pso);
    D3D12_RESOURCE_BARRIER b = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION};
    b.Transition = {bb[bi], 0, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET};
    cl->ResourceBarrier(1, &b);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_heap->GetCPUDescriptorHandleForHeapStart(); rtv.ptr += bi * rtv_inc;
    float clear[4] = {0.05f, 0.05f, 0.08f, 1};
    cl->ClearRenderTargetView(rtv, clear, 0, nullptr);
    cl->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    D3D12_VIEWPORT vp = {0, 0, (float)g_w, (float)g_h, 0, 1}; D3D12_RECT sr = {0, 0, g_w, g_h};
    cl->RSSetViewports(1, &vp); cl->RSSetScissorRects(1, &sr);
    cl->SetGraphicsRootSignature(root);
    ID3D12DescriptorHeap *heaps[] = {srv_heap}; cl->SetDescriptorHeaps(1, heaps);
    cl->SetGraphicsRootDescriptorTable(1, srv_heap->GetGPUDescriptorHandleForHeapStart());
    cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    if (g_indirect && csig && argbuf) {
      cl->ExecuteIndirect(csig, g_draws, argbuf, 0, nullptr, 0);
    } else {
      for (int d = 0; d < g_draws; d++) {
        float rcst[8] = {(d % grid + 0.5f) / grid * 2 - 1, (d / grid + 0.5f) / grid * 2 - 1, 0.9f / grid, (float)t + d * 0.01f,
                         0.5f + 0.5f * sinf(d * 0.1f), 0.8f, 1, 1};
        cl->SetGraphicsRoot32BitConstants(0, 8, rcst, 0);
        cl->DrawInstanced(6, 1, 0, 0);
      }
    }
    if (g_overdraw) {
      cl->SetPipelineState(fpso);
      float rcst[8] = {0, 0, 1, 0, 1, 1, 1, 1};
      cl->SetGraphicsRoot32BitConstants(0, 8, rcst, 0);
      for (int k = 0; k < g_overdraw; k++) cl->DrawInstanced(3, 1, 0, 0);
    }
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cl->ResourceBarrier(1, &b);
    cl->Close();
    queue->ExecuteCommandLists(1, lists);
    sc->Present(0, 0);
    queue->Signal(fence, ++fv); frame_fence[bi] = fv;
    QueryPerformanceCounter(&tn);
    double now = double(tn.QuadPart - t0.QuadPart) / freq.QuadPart;
    if (frame) ms.push_back((now - prev) * 1000.0);
    prev = now;
  }
  queue->Signal(fence, ++fv); fence->SetEventOnCompletion(fv, ev); WaitForSingleObject(ev, 5000);
  std::vector<double> v; double acc = 0;
  for (double x : ms) { acc += x; if (acc > 1000.0) v.push_back(x); }
  if (v.empty()) { printf("%s: no frames\n", g_label); return 3; }
  std::vector<double> s = v; std::sort(s.begin(), s.end());
  double sum = 0; for (double x : v) sum += x;
  auto pct = [&](double p) { return s[std::min(s.size() - 1, (size_t)(p * (s.size() - 1)))]; };
  size_t n1 = std::max<size_t>(1, s.size() / 100); double worst = 0;
  for (size_t i = s.size() - n1; i < s.size(); i++) worst += s[i];
  printf("RESULT label=%s api=d3d12 draws=%d overdraw=%d indirect=%d %dx%d frames=%zu avg_fps=%.1f p50_ms=%.2f p95_ms=%.2f "
         "p99_ms=%.2f low1_fps=%.1f max_ms=%.1f\n", g_label, g_draws, g_overdraw, g_indirect, g_w, g_h, v.size(),
         1000.0 * v.size() / sum, pct(0.5), pct(0.95), pct(0.99), 1000.0 / (worst / n1), s.back());
  fflush(stdout);
  return 0;
}
