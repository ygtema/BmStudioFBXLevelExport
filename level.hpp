// BmStudio - level viewport (.umap): shows the placed static meshes and lets you move/rotate/scale/hide
// instances and actors WITHOUT creating new objects (only changes matrix bytes or tags of known types) -> "cooked limits".
// Included by app.cpp after viewer.hpp.
#pragma once
#include "../bmcpp/bmlevel.hpp"
#include "../bmcpp/bmlevelfbx.hpp"

struct LvlGpuMesh { IDirect3DVertexBuffer9* vb = nullptr; IDirect3DIndexBuffer9* ib = nullptr; u32 numVerts = 0; std::vector<DrawSection> secs;
    float bmin[3] = { 0, 0, 0 }, bmax[3] = { 0, 0, 0 }; std::vector<float> pos; std::vector<uint32_t> idx; bool ok = false; std::string err; };
// GPU resources of ONE level package (the document or a context sub-level)
struct LvlCache {
    const Package* pk = nullptr; std::wstring path;
    std::map<i32, LvlGpuMesh> meshes; std::map<i32, IDirect3DTexture9*> tex; std::vector<LvlGpuMesh> bsp, terrain;
    std::vector<i32> toLoad; size_t loaded = 0;
};
struct LvlUndo { int kind; int index; M4 world; float loc[3]; i32 rot[3]; bool hidden; };   // kind 0 mesh, 1 actor
struct LvlActor { i32 idx; float loc[3]; Rot rot; std::string cls; };
struct LevelState {
    int gen = -1; Level L; std::vector<M4> orig; std::vector<LvlActor> actors; LvlCache main;
    float eye[3] = { 0, 0, 0 }, yaw = 0, pitch = -0.3f, speed = 800; bool camInit = false;
    int sel = -1, selActor = -1; int mode = 0;   // 0 move 1 rotate 2 scale
    bool showBsp = true, showTerrain = true, showTrees = false;
    bool textured = true, showHidden = true, showActors = true, snap = false; float snapMove = 16, snapRot = 15; int texMax = 256;
    int dragAxis = -1; bool dragging = false; std::vector<LvlUndo> undo; char filter[128] = { 0 };
    IDirect3DTexture9* rt = nullptr; IDirect3DSurface9* ds = nullptr; int rtW = 0, rtH = 0;
    Mat4 view, proj; int vw = 0, vh = 0;
};
static LevelState gLvl;
static bool gLevelMode = false;

// context sub-levels (display only): the other packages of the same area, opened alongside the document
struct LvlCtxLayer { std::string name; std::wstring path; std::unique_ptr<Doc> doc; LvlCache c; Level L; bool show = true, loaded = false, failed = false; std::string err; };
struct LvlCtxState {
    std::wstring forDir; std::map<std::string, std::wstring> index;      // package name (lowercase) -> .umap file
    std::string family; std::vector<std::string> parents;                  // chosen parent + parent chain
    std::vector<std::unique_ptr<LvlCtxLayer>> layers; bool enabled = true; std::string status; int texMax = 0; std::wstring builtFor; int up = 0;
};
static LvlCtxState gCtx;

// "Import room": the source room drawn over the map, offset (filled in by roomimp.hpp)
struct LvlGhostLayer { LvlCache* c; const Level* L; };
struct LvlGhost { bool on = false, sel = false, boxOn = true, hasBox = false; int yaw90 = 0; float off[3] = { 0, 0, 0 }, bmn[3] = { 0, 0, 0 }, bmx[3] = { 0, 0, 0 }; std::vector<LvlGhostLayer> layers; };
static LvlGhost gGhost;
// ghost room matrix (row vector): rotates yaw90*90 degrees around the box center and offsets by 'off' (same as rimp::Xf)
static M4 GhostMatrix() { float c[3] = { 0, 0, 0 }; if (gGhost.hasBox) for (int k = 0; k < 3; k++) c[k] = (gGhost.bmn[k] + gGhost.bmx[k]) * 0.5f;
    static const float cs[4] = { 1, 0, -1, 0 }, sn[4] = { 0, 1, 0, -1 }; int q = gGhost.yaw90 & 3; M4 R = m4Identity(); R.m[0][0] = cs[q]; R.m[0][1] = sn[q]; R.m[1][0] = -sn[q]; R.m[1][1] = cs[q];
    return m4Mul(m4Mul(translationMatrix(-c[0], -c[1], -c[2]), R), translationMatrix(c[0] + gGhost.off[0], c[1] + gGhost.off[1], c[2] + gGhost.off[2])); }
static void RimpOpen();
static void LibOpen();
struct CollStateFwd; static bool CollIsOn(); static void CollSetOn(bool on); static void CollPanel(); static void CollPick(float mx, float my); static void CollOverlay(ImDrawList* dl, ImVec2 org); static void CollUndoLast();
static bool LvlMeshNoCollision(int i); static void LvlSetNoCollision(int i, bool nc);
static void HolePanel(); static bool HolePlacing(); static void HolePick(float mx, float my); static void HoleOverlay(ImDrawList* dl, ImVec2 org);

static void LvlCacheRelease(LvlCache& c) {
    for (auto& kv : c.meshes) { if (kv.second.vb) kv.second.vb->Release(); if (kv.second.ib) kv.second.ib->Release(); }
    for (auto* v : { &c.bsp, &c.terrain }) for (auto& b : *v) { if (b.vb) b.vb->Release(); if (b.ib) b.ib->Release(); }
    for (auto& kv : c.tex) if (kv.second) kv.second->Release();
    const Package* pk = c.pk; std::wstring path = c.path; c = LvlCache(); c.pk = pk; c.path = path;
}
static void LevelReleaseRT() { if (gLvl.rt) gLvl.rt->Release(); if (gLvl.ds) gLvl.ds->Release(); gLvl.rt = nullptr; gLvl.ds = nullptr; gLvl.rtW = gLvl.rtH = 0; }
static void LevelClear() {
    LvlCacheRelease(gLvl.main);
    LevelReleaseRT(); float eye[3]; memcpy(eye, gLvl.eye, 12); float y = gLvl.yaw, p = gLvl.pitch, s = gLvl.speed; bool ci = gLvl.camInit; int tm = gLvl.texMax;
    bool sb = gLvl.showBsp, st = gLvl.showTerrain, str = gLvl.showTrees; gLvl = LevelState(); gLvl.showBsp = sb; gLvl.showTerrain = st; gLvl.showTrees = str;
    memcpy(gLvl.eye, eye, 12); gLvl.yaw = y; gLvl.pitch = p; gLvl.speed = s; gLvl.camInit = ci; gLvl.texMax = tm;
}
static void LvlCtxClear() { for (auto& l : gCtx.layers) LvlCacheRelease(l->c); gCtx = LvlCtxState(); }
static bool PackageHasLevel() { static int gen = -2; static bool has = false; if (!gDoc.loaded || gDoc.pk.bigEndian) return false; if (gen == gDocGen) return has; gen = gDocGen; has = false;
    for (auto& e : gDoc.pk.exports) if (gDoc.pk.nm(e.name) == "PersistentLevel") { has = true; break; } return has; }

static IDirect3DTexture9* LvlTexture(LvlCache& c, i32 mat) {
    const Package& pk = *c.pk;
    i32 t = 0; try { t = findDiffuseTexture(pk, mat); } catch (...) {} if (t <= 0 || pk.classOf(pk.exports[t - 1]) != "Texture2D") return nullptr;
    auto it = c.tex.find(t); if (it != c.tex.end()) return it->second;
    IDirect3DTexture9* tx = nullptr;
    try { TextureInfo ti = parseTexture(pk, t);
        if (!ti.mips.empty()) { int want = 0; while (want + 1 < (int)ti.mips.size() && std::max(ti.mips[want].w, ti.mips[want].h) > gLvl.texMax) want++;
            int got = -1; std::vector<u8> rgba; if (LoadTextureRGBA(t, ti, want, got, rgba, nullptr, &pk, &c.path)) tx = MakeTexture(rgba, ti.mips[got].w, ti.mips[got].h); } } catch (...) {}
    c.tex[t] = tx; return tx;
}
static bool LvlUpload(LvlGpuMesh& g, const std::vector<GpuVert>& vs) {
    void* p = nullptr; g.numVerts = (u32)vs.size();
    if (vs.empty() || g.idx.empty()) { g.err = "empty"; return false; }
    if (FAILED(gDev->CreateVertexBuffer((UINT)(vs.size() * sizeof(GpuVert)), D3DUSAGE_WRITEONLY, GPUVERT_FVF, D3DPOOL_MANAGED, &g.vb, NULL))) { g.err = "VB"; return false; }
    g.vb->Lock(0, 0, &p, 0); memcpy(p, vs.data(), vs.size() * sizeof(GpuVert)); g.vb->Unlock();
    if (FAILED(gDev->CreateIndexBuffer((UINT)(g.idx.size() * 4), D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_MANAGED, &g.ib, NULL))) { g.vb->Release(); g.vb = nullptr; g.err = "IB"; return false; }
    g.ib->Lock(0, 0, &p, 0); memcpy(p, g.idx.data(), g.idx.size() * 4); g.ib->Unlock(); g.ok = true; return true;
}
static void LvlBuildMesh(LvlCache& c, i32 mesh) {
    LvlGpuMesh& g = c.meshes[mesh]; const Package& pk = *c.pk;
    if (mesh <= 0) { g.err = "import (another package)"; return; }
    std::vector<GpuVert> vs; for (int k = 0; k < 3; k++) { g.bmin[k] = 1e30f; g.bmax[k] = -1e30f; }
    auto addV = [&](const float* p3, const float* n, const float* uv) { vs.push_back({ p3[0], p3[1], p3[2], n[0], n[1], n[2], uv[0], uv[1] }); for (int k = 0; k < 3; k++) { g.bmin[k] = std::min(g.bmin[k], p3[k]); g.bmax[k] = std::max(g.bmax[k], p3[k]); g.pos.push_back(p3[k]); } };
    if (pk.classOf(pk.exports[mesh - 1]) == "SkeletalMesh") {                         // reference pose
        SkeletalMesh sm; try { sm = parseSkeletalMesh(pk, mesh); } catch (std::exception& e) { sm.err = e.what(); }
        if (!sm.err.empty() || sm.lods.empty()) { g.err = sm.err.empty() ? "no LODs" : sm.err; return; }
        const SkelLOD& l = sm.lods[0]; for (auto& v : l.verts) addV(v.pos, v.normal, v.uv); g.idx = l.indices;
        for (auto& s : l.sections) { DrawSection d; d.first = s.baseIndex; d.tris = s.numTris; d.minV = 0; d.maxV = (u32)l.verts.size() - 1;
            i32 m = (s.material < (int)sm.materialRefs.size()) ? sm.materialRefs[s.material] : 0; d.material = m ? pk.objPath(m) : "None"; d.tex = m ? LvlTexture(c, m) : nullptr;
            try { d.blend = materialBlendMode(pk, m); } catch (...) {} g.secs.push_back(d); }
        LvlUpload(g, vs); return;
    }
    StaticMesh st; try { st = parseStaticMesh(pk, mesh); } catch (std::exception& e) { st.err = e.what(); }
    if (!st.err.empty() || st.lods.empty()) { g.err = st.err.empty() ? "no LODs" : st.err; return; }
    const StaticLOD& l = st.lods[0]; if (l.verts.empty() || l.indices.empty()) { g.err = "empty"; return; }
    vs.reserve(l.verts.size()); for (auto& v : l.verts) addV(v.pos, v.normal, v.uv);
    g.idx = l.indices;
    for (auto& e : l.elements) { DrawSection d; d.first = e.firstIndex; d.tris = e.numTris; d.minV = e.minV; d.maxV = e.maxV; d.material = e.materialPath; d.tex = LvlTexture(c, e.material); try { d.blend = materialBlendMode(pk, e.material); } catch (...) {} g.secs.push_back(d); }
    LvlUpload(g, vs);
}
static void LvlBuildFlat(LvlCache& c, std::vector<LvlGpuMesh>& out, const std::vector<float>& pos, const std::vector<uint32_t>& idx, const std::vector<std::pair<i32, std::pair<u32, u32>>>* secs, const std::vector<float>* uvs) {
    LvlGpuMesh g; std::vector<float> nrm(pos.size(), 0.f); size_t nv = pos.size() / 3;
    for (size_t t = 0; t + 2 < idx.size(); t += 3) { const float *a = &pos[idx[t] * 3], *b = &pos[idx[t + 1] * 3], *cc = &pos[idx[t + 2] * 3];
        float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { cc[0] - a[0], cc[1] - a[1], cc[2] - a[2] }; float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
        for (int j = 0; j < 3; j++) for (int k = 0; k < 3; k++) nrm[idx[t + j] * 3 + k] += n[k]; }
    std::vector<GpuVert> vs; vs.reserve(nv);
    for (size_t i = 0; i < nv; i++) { float u = uvs ? (*uvs)[i * 2] : pos[i * 3] / 512.f, v = uvs ? (*uvs)[i * 2 + 1] : pos[i * 3 + 1] / 512.f;
        vs.push_back({ pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2], nrm[i * 3], nrm[i * 3 + 1], nrm[i * 3 + 2], u, v }); }
    g.idx = idx;
    if (secs) for (auto& s : *secs) { DrawSection d; d.first = s.second.first; d.tris = s.second.second; d.minV = 0; d.maxV = (u32)nv - 1; d.material = s.first ? c.pk->objPath(s.first) : "None";
        d.tex = s.first > 0 ? LvlTexture(c, s.first) : nullptr; try { d.blend = materialBlendMode(*c.pk, s.first); } catch (...) {} if (d.blend >= 2) d.blend = 0; g.secs.push_back(d); }
    else { DrawSection d; d.first = 0; d.tris = (u32)idx.size() / 3; d.minV = 0; d.maxV = (u32)nv - 1; d.material = "Terrain"; d.tex = nullptr; g.secs.push_back(d); }
    if (LvlUpload(g, vs)) out.push_back(g);
}
static void LvlBuildBsp(LvlCache& c, i32 model) {
    BspModel bm = parseModel(*c.pk, model); if (!bm.err.empty() || bm.verts.empty()) return;
    std::vector<float> pos, uv; for (auto& v : bm.verts) { pos.insert(pos.end(), v.pos, v.pos + 3); uv.push_back(v.uv[0]); uv.push_back(v.uv[1]); }
    LvlBuildFlat(c, c.bsp, pos, bm.idx, &bm.secs, &uv);
}
static void LvlBuildTerrain(LvlCache& c, i32 t) {
    static float def[3] = { 256, 256, 256 };
    TerrainMesh tm = parseTerrain(*c.pk, t, def); if (!tm.err.empty() || tm.idx.empty()) return;
    LvlBuildFlat(c, c.terrain, tm.pos, tm.idx, nullptr, nullptr);
}
// loads a level into a cache (BSP and terrain right away; meshes queued)
static void LvlLoadInto(LvlCache& c, const Level& L) {
    { bool betaRoom = c.pk->findPath("BetaBSP") > 0;   // beta room: the level BSP is collision only (the visible part is in the BetaBSP.* meshes)
      if (!betaRoom) for (i32 mdl : levelModels(*c.pk)) LvlBuildBsp(c, mdl); }
    for (i32 t : L.terrains) LvlBuildTerrain(c, t);
    std::set<i32> uniq; for (auto& m : L.meshes) if (!uniq.count(m.mesh)) { uniq.insert(m.mesh); c.toLoad.push_back(m.mesh); }
}
// placeholder tree (SpeedTree is not drawn): trunk + canopy, ~900 units
static LvlGpuMesh gTreeTrunk, gTreeTop;
static void LvlBuildTree() {
    if (gTreeTrunk.ok) return;
    auto cone = [](LvlGpuMesh& g, float r0, float r1, float z0, float z1) { std::vector<GpuVert> vs; const int N = 10;
        for (int i = 0; i <= N; i++) { float a = i * 6.2831853f / N, cx = cosf(a), sy = sinf(a);
            vs.push_back({ cx * r0, sy * r0, z0, cx, sy, 0.3f, 0, 0 }); vs.push_back({ cx * r1, sy * r1, z1, cx, sy, 0.3f, 0, 1 }); }
        for (int i = 0; i < N; i++) { uint32_t a = i * 2; g.idx.insert(g.idx.end(), { a, a + 2, a + 1, a + 1, a + 2, a + 3 }); }
        DrawSection d; d.first = 0; d.tris = (u32)g.idx.size() / 3; d.minV = 0; d.maxV = (u32)vs.size() - 1; g.secs.push_back(d); LvlUpload(g, vs); };
    cone(gTreeTrunk, 10, 6, 0, 160); cone(gTreeTop, 70, 2, 120, 360);
}
static void LevelLoad() {
    LevelClear(); gLvl.gen = gDocGen; gLvl.main.pk = &gDoc.pk; gLvl.main.path = gDoc.path;
    gLvl.L = parseLevel(gDoc.pk); for (auto& m : gLvl.L.meshes) gLvl.orig.push_back(m.world);
    LvlLoadInto(gLvl.main, gLvl.L);
    std::set<i32> meshOwners; for (auto& m : gLvl.L.meshes) meshOwners.insert(m.owner);
    for (i32 a : gLvl.L.actors) { ObjProps op = objProps(gDoc.pk, a); if (!op.ok) continue; LvlActor la; la.idx = a; la.cls = gDoc.pk.classOf(gDoc.pk.exports[a - 1]);
        if (la.cls == "StaticMeshCollectionActor" || meshOwners.count(a)) continue;
        if (!tagVec(op.tags, "Location", la.loc)) continue; tagRot(op.tags, "Rotation", la.rot); gLvl.actors.push_back(la); }
    if (!gLvl.camInit && !gLvl.L.meshes.empty()) {   // camera near the center (median: ignores objects parked far away)
        float mn[3], mx[3]; for (int k = 0; k < 3; k++) { std::vector<float> v; for (auto& m : gLvl.L.meshes) v.push_back(m.world.m[3][k]); std::sort(v.begin(), v.end()); mn[k] = v[v.size() / 10]; mx[k] = v[v.size() * 9 / 10]; }
        gLvl.eye[0] = (mn[0] + mx[0]) / 2 - (mx[0] - mn[0]) * 0.6f; gLvl.eye[1] = (mn[1] + mx[1]) / 2; gLvl.eye[2] = mx[2] + (mx[0] - mn[0]) * 0.45f;
        gLvl.yaw = 0; gLvl.pitch = -0.55f; gLvl.speed = std::max(400.f, (mx[0] - mn[0]) * 0.1f); gLvl.camInit = true;
    }
    { static bool camEnv = false; wchar_t b[256]; if (!camEnv && GetEnvironmentVariableW(L"BMS_CAM", b, 256)) { camEnv = true; swscanf(b, L"%f,%f,%f,%f,%f", &gLvl.eye[0], &gLvl.eye[1], &gLvl.eye[2], &gLvl.yaw, &gLvl.pitch); gLvl.camInit = true; } }   // test
}

// ------------------------------------------------------------------ context sub-levels
static std::string LvlLower(std::string s) { for (auto& ch : s) ch = (char)tolower((unsigned char)ch); return s; }
static std::string LvlPkgName(const std::wstring& path) { std::wstring f = path.substr(path.find_last_of(L"\\/") + 1); size_t d = f.rfind(L'.'); return U8(d == std::wstring::npos ? f : f.substr(0, d)); }
static void LvlIndexMaps(const std::wstring& dir) {
    gCtx.index.clear(); std::vector<std::wstring> stack{ dir };
    while (!stack.empty()) { std::wstring d = stack.back(); stack.pop_back(); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((d + L"\\*").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) continue;
        do { std::wstring n = fd.cFileName; if (n == L"." || n == L"..") continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) stack.push_back(d + L"\\" + n);
            else if (n.size() > 5 && _wcsicmp(n.c_str() + n.size() - 5, L".umap") == 0) gCtx.index[LvlLower(LvlPkgName(n))] = d + L"\\" + n;
        } while (FindNextFileW(h, &fd)); FindClose(h); }
}
// sub-levels (LevelStreaming*.PackageName) of a map package
static std::vector<std::string> LvlStreamingOf(const Package& pk) {
    std::vector<std::string> out; for (size_t k = 0; k < pk.exports.size(); k++) { std::string c = pk.classOf(pk.exports[k]); if (c.rfind("LevelStreaming", 0) != 0) continue;
        ObjProps op = objProps(pk, (i32)k + 1); if (!op.ok) continue; std::string n = tagName(pk, op.tags, "PackageName"); if (!n.empty() && n != "None") out.push_back(n); }
    return out;
}
static std::vector<std::string> LvlStreamingOfFile(const std::wstring& path) {
    if (_wcsicmp(path.c_str(), gDoc.path.c_str()) == 0) return LvlStreamingOf(gDoc.pk);
    Doc d; if (!d.open(path).empty()) return {}; return LvlStreamingOf(d.pk);
}
// builds the document family: the closest parent (name prefix) that lists this package among its sub-levels; up = how many levels to go up
static void LvlBuildFamily(int up) {
    std::wstring maps = W(cookedDirOf(U8(gDoc.path))) + L"Maps"; if (GetFileAttributesW(maps.c_str()) == INVALID_FILE_ATTRIBUTES) { size_t s = gDoc.path.find_last_of(L"\\/"); maps = gDoc.path.substr(0, s); }
    if (gCtx.forDir != maps) { LvlIndexMaps(maps); gCtx.forDir = maps; }
    std::string me = LvlPkgName(gDoc.path); std::vector<std::string> chain;
    { std::vector<std::string> own = LvlStreamingOf(gDoc.pk); if (!own.empty()) chain.push_back(me); }
    std::string cur = me;
    for (;;) { size_t u = cur.rfind('_'); if (u == std::string::npos) break; cur = cur.substr(0, u);
        auto it = gCtx.index.find(LvlLower(cur)); if (it == gCtx.index.end()) continue;
        std::vector<std::string> sl = LvlStreamingOfFile(it->second); bool has = false; for (auto& x : sl) if (LvlLower(x) == LvlLower(me) || (!chain.empty() && LvlLower(x) == LvlLower(chain.back()))) has = true;
        if (has) chain.push_back(cur); }
    gCtx.parents = chain; gCtx.builtFor = gDoc.path;
    for (auto& l : gCtx.layers) LvlCacheRelease(l->c); gCtx.layers.clear();
    if (chain.empty()) { gCtx.family.clear(); gCtx.status = "no parent map found for " + me; return; }
    std::string parent = chain[std::min((size_t)up, chain.size() - 1)]; gCtx.family = parent;
    std::vector<std::string> members{ parent }; for (auto& x : LvlStreamingOfFile(gCtx.index[LvlLower(parent)])) members.push_back(x);
    std::set<std::string> seen;
    for (auto& n : members) { std::string k = LvlLower(n); if (seen.count(k)) continue; seen.insert(k);
        auto l = std::make_unique<LvlCtxLayer>(); l->name = n; auto it = gCtx.index.find(k); if (it == gCtx.index.end()) { l->failed = true; l->err = "file not found"; l->show = false; } else l->path = it->second;
        std::string lk = LvlLower(n); if (lk.size() > 6 && (lk.rfind("_audio") == lk.size() - 6)) l->show = false;      // no geometry
        gCtx.layers.push_back(std::move(l)); }
    gCtx.status.clear();
}
static bool LvlIsCurrent(const LvlCtxLayer& l) { return !l.path.empty() && _wcsicmp(l.path.c_str(), gDoc.path.c_str()) == 0; }
// releases the file (mapping) of a context sub-level: the document is going to be saved over it
static void LvlCtxReleasePath(const std::wstring& path) {
    for (auto& l : gCtx.layers) if (l->doc && _wcsicmp(l->path.c_str(), path.c_str()) == 0) { LvlCacheRelease(l->c); l->L = Level(); l->doc.reset(); l->loaded = false; } }
// loads 1 sub-level per call (open, parse, BSP/terrain); meshes go through the cache queue
static void LvlCtxStep(double budget) {
    double t0 = ImGui::GetTime();
    if (gCtx.texMax != gLvl.texMax) { for (auto& l : gCtx.layers) { LvlCacheRelease(l->c); l->loaded = false; } gCtx.texMax = gLvl.texMax; }
    for (auto& lp : gCtx.layers) { LvlCtxLayer& l = *lp; if (ImGui::GetTime() - t0 > budget) return;
        if (LvlIsCurrent(l) && l.doc) { LvlCtxReleasePath(l.path); continue; }
        if (!l.show || l.failed || LvlIsCurrent(l)) continue;
        if (!l.loaded) {
            if (!l.doc) { l.doc = std::make_unique<Doc>(); std::string e = l.doc->open(l.path); if (!e.empty()) { l.failed = true; l.err = e; l.doc.reset(); continue; } }
            l.c.pk = &l.doc->pk; l.c.path = l.path; l.L = parseLevel(l.doc->pk); LvlLoadInto(l.c, l.L); l.loaded = true; continue; }
        while (l.c.loaded < l.c.toLoad.size() && ImGui::GetTime() - t0 < budget) { LvlBuildMesh(l.c, l.c.toLoad[l.c.loaded]); l.c.loaded++; }
    }
}

// ------------------------------------------------------------------ camera/picking math
static void LvlDir(float d[3]) { d[0] = cosf(gLvl.pitch) * cosf(gLvl.yaw); d[1] = cosf(gLvl.pitch) * sinf(gLvl.yaw); d[2] = sinf(gLvl.pitch); }
static bool LvlProject(const float p[3], float& sx, float& sy) {   // world -> viewport pixels
    float v[4] = { p[0], p[1], p[2], 1 }, c[4] = { 0, 0, 0, 0 }; Mat4 vp = MatMul(gLvl.view, gLvl.proj);
    for (int j = 0; j < 4; j++) for (int k = 0; k < 4; k++) c[j] += v[k] * vp.m[k][j];
    if (c[3] <= 1e-3f) return false; sx = (c[0] / c[3] * 0.5f + 0.5f) * gLvl.vw; sy = (0.5f - c[1] / c[3] * 0.5f) * gLvl.vh; return true;
}
static void LvlRay(float mx, float my, float o[3], float d[3]) {
    float x = (2 * mx / gLvl.vw - 1) / gLvl.proj.m[0][0], y = (1 - 2 * my / gLvl.vh) / gLvl.proj.m[1][1];
    const Mat4& v = gLvl.view; // view = camera rows; inverse of the rotation = transpose
    for (int k = 0; k < 3; k++) d[k] = x * v.m[k][0] + y * v.m[k][1] + 1 * v.m[k][2];
    float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); for (int k = 0; k < 3; k++) { d[k] /= l; o[k] = gLvl.eye[k]; }
}
static bool RayTri(const float o[3], const float d[3], const float* a, const float* b, const float* c, float& t) {
    float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
    float p[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] }; float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (fabsf(det) < 1e-9f) return false; float inv = 1 / det; float s[3] = { o[0] - a[0], o[1] - a[1], o[2] - a[2] }; float u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv; if (u < 0 || u > 1) return false;
    float q[3] = { s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] }; float v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv; if (v < 0 || u + v > 1) return false;
    t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv; return t > 0;
}
static bool RayBox(const float o[3], const float d[3], const float mn[3], const float mx[3]) {
    float t0 = 0, t1 = 1e30f; for (int k = 0; k < 3; k++) { if (fabsf(d[k]) < 1e-12f) { if (o[k] < mn[k] || o[k] > mx[k]) return false; continue; }
        float a = (mn[k] - o[k]) / d[k], b = (mx[k] - o[k]) / d[k]; if (a > b) std::swap(a, b); t0 = std::max(t0, a); t1 = std::min(t1, b); if (t0 > t1) return false; } return true;
}
static void LvlPick(float mx, float my) {
    float o[3], d[3]; LvlRay(mx, my, o, d); float best = 1e30f; int bi = -1, ba = -1;
    gGhost.sel = false;
    if (gGhost.on) {   // room being imported: clicking it selects the whole room (move gizmo)
        float gb = 1e30f; M4 gi = m4Inverse(GhostMatrix()); float go[3], gd[3];
        for (int k = 0; k < 3; k++) { go[k] = o[0] * gi.m[0][k] + o[1] * gi.m[1][k] + o[2] * gi.m[2][k] + gi.m[3][k]; gd[k] = d[0] * gi.m[0][k] + d[1] * gi.m[1][k] + d[2] * gi.m[2][k]; }
        for (auto& gl : gGhost.layers) for (auto& m : gl.L->meshes) { if (m.hidden) continue; auto it = gl.c->meshes.find(m.mesh); if (it == gl.c->meshes.end() || !it->second.ok) continue; const LvlGpuMesh& g = it->second;
            M4 inv = m4Inverse(m.world); float lo[3], ld[3];
            for (int k = 0; k < 3; k++) { lo[k] = go[0] * inv.m[0][k] + go[1] * inv.m[1][k] + go[2] * inv.m[2][k] + inv.m[3][k]; ld[k] = gd[0] * inv.m[0][k] + gd[1] * inv.m[1][k] + gd[2] * inv.m[2][k]; }
            if (!RayBox(lo, ld, g.bmin, g.bmax)) continue;
            for (size_t t = 0; t + 2 < g.idx.size(); t += 3) { float tt; if (!RayTri(lo, ld, &g.pos[g.idx[t] * 3], &g.pos[g.idx[t + 1] * 3], &g.pos[g.idx[t + 2] * 3], tt)) continue;
                float hp[3], wp[3]; for (int k = 0; k < 3; k++) hp[k] = lo[k] + ld[k] * tt;
                for (int k = 0; k < 3; k++) wp[k] = hp[0] * m.world.m[0][k] + hp[1] * m.world.m[1][k] + hp[2] * m.world.m[2][k] + m.world.m[3][k];
                float dist = sqrtf((wp[0] - go[0]) * (wp[0] - go[0]) + (wp[1] - go[1]) * (wp[1] - go[1]) + (wp[2] - go[2]) * (wp[2] - go[2])); gb = std::min(gb, dist); } }
        best = gb;   // the map mesh only wins if it is in front
    }
    for (size_t i = 0; i < gLvl.L.meshes.size(); i++) {
        const LevelMesh& m = gLvl.L.meshes[i]; if (m.hidden && !gLvl.showHidden) continue; auto it = gLvl.main.meshes.find(m.mesh); if (it == gLvl.main.meshes.end() || !it->second.ok) continue; const LvlGpuMesh& g = it->second;
        M4 inv = m4Inverse(m.world); float lo[3], ld[3];
        for (int k = 0; k < 3; k++) { lo[k] = o[0] * inv.m[0][k] + o[1] * inv.m[1][k] + o[2] * inv.m[2][k] + inv.m[3][k]; ld[k] = d[0] * inv.m[0][k] + d[1] * inv.m[1][k] + d[2] * inv.m[2][k]; }
        if (!RayBox(lo, ld, g.bmin, g.bmax)) continue;
        for (size_t t = 0; t + 2 < g.idx.size(); t += 3) { float tt; if (!RayTri(lo, ld, &g.pos[g.idx[t] * 3], &g.pos[g.idx[t + 1] * 3], &g.pos[g.idx[t + 2] * 3], tt)) continue;
            float hp[3], wp[3]; for (int k = 0; k < 3; k++) hp[k] = lo[k] + ld[k] * tt;
            for (int k = 0; k < 3; k++) wp[k] = hp[0] * m.world.m[0][k] + hp[1] * m.world.m[1][k] + hp[2] * m.world.m[2][k] + m.world.m[3][k];
            float dist = sqrtf((wp[0] - o[0]) * (wp[0] - o[0]) + (wp[1] - o[1]) * (wp[1] - o[1]) + (wp[2] - o[2]) * (wp[2] - o[2])); if (dist < best) { best = dist; bi = (int)i; } }
    }
    if (gLvl.showActors) for (size_t a = 0; a < gLvl.actors.size(); a++) { float sx, sy; if (!LvlProject(gLvl.actors[a].loc, sx, sy)) continue;
        if ((sx - mx) * (sx - mx) + (sy - my) * (sy - my) < 64) { float dd[3] = { gLvl.actors[a].loc[0] - o[0], gLvl.actors[a].loc[1] - o[1], gLvl.actors[a].loc[2] - o[2] }; float dist = sqrtf(dd[0] * dd[0] + dd[1] * dd[1] + dd[2] * dd[2]); if (dist < best) { best = dist; ba = (int)a; bi = -1; } } }
    gLvl.sel = bi; gLvl.selActor = ba;
    if (gGhost.on && bi < 0 && ba < 0 && best < 1e30f) { gGhost.sel = true; gLvl.mode = 0; }
}

// ------------------------------------------------------------------ editing
static void LvlPushUndo() {
    LvlUndo u = {}; if (gLvl.sel >= 0) { u.kind = 0; u.index = gLvl.sel; u.world = gLvl.L.meshes[gLvl.sel].world; u.hidden = gLvl.L.meshes[gLvl.sel].hidden; }
    else if (gLvl.selActor >= 0) { u.kind = 1; u.index = gLvl.selActor; memcpy(u.loc, gLvl.actors[gLvl.selActor].loc, 12); u.rot[0] = gLvl.actors[gLvl.selActor].rot.pitch; u.rot[1] = gLvl.actors[gLvl.selActor].rot.yaw; u.rot[2] = gLvl.actors[gLvl.selActor].rot.roll; }
    else return; gLvl.undo.push_back(u); if (gLvl.undo.size() > 200) gLvl.undo.erase(gLvl.undo.begin());
}
static bool LvlSetMeshWorld(int i, const M4& w) { bool ok = setInstanceWorld(gDoc.pk, gLvl.L.meshes[i], w); if (ok) gDoc.dirty = true; else Status("edit failed"); return ok; }
static bool LvlSetActor(int a, const float loc[3], const Rot& r) {
    LvlActor& la = gLvl.actors[a]; ObjProps op = objProps(gDoc.pk, la.idx); if (!op.ok) return false; std::vector<Tag> tags = op.tags;
    setStructTag(tags, "Location", "Vector", loc, 12); i32 rr[3] = { r.pitch, r.yaw, r.roll }; if (findTag(tags, "Rotation") || r.pitch || r.yaw || r.roll) setStructTag(tags, "Rotation", "Rotator", rr, 12);
    if (!replaceProps(gDoc.pk, la.idx, tags)) { Status("edit failed"); return false; } memcpy(la.loc, loc, 12); la.rot = r; gDoc.dirty = true;
    // light: the cached matrix of the component follows the actor
    if (const Tag* lc = findTag(tags, "LightComponent")) { i32 c; memcpy(&c, lc->raw.data(), 4); if (c > 0 && c <= (i32)gDoc.pk.exports.size()) { ObjProps cp = objProps(gDoc.pk, c);
        if (cp.ok && findTag(cp.tags, "CachedParentToWorld")) { std::vector<Tag> ct = cp.tags; M4 m = actorMatrix(tags); setStructTag(ct, "CachedParentToWorld", "Matrix", &m, 64); replaceProps(gDoc.pk, c, ct); } } }
    return true;
}
// hide in game: HiddenGame on the component (+ no collision)
static void LvlSetHidden(int i, bool hide) {
    LevelMesh& m = gLvl.L.meshes[i]; ObjProps cp = objProps(gDoc.pk, m.comp); if (!cp.ok) { Status("edit failed"); return; }
    std::vector<Tag> tags = cp.tags; setBoolTag(tags, "HiddenGame", hide);
    for (const char* b : { "CollideActors", "BlockActors", "BlockZeroExtent", "BlockNonZeroExtent", "BlockRigidBody" }) { if (hide) setBoolTag(tags, b, false); else for (size_t k = 0; k < tags.size(); k++) if (tags[k].name == b && !tags[k].boolVal) { tags.erase(tags.begin() + k); break; } }
    if (!hide) for (size_t k = 0; k < tags.size(); k++) if (tags[k].name == "HiddenGame") { tags.erase(tags.begin() + k); break; }
    if (replaceProps(gDoc.pk, m.comp, tags)) { m.hidden = hide; gDoc.dirty = true; } else Status("edit failed");
}
static void LvlUndoLast() {
    if (gLvl.undo.empty()) return; LvlUndo u = gLvl.undo.back(); gLvl.undo.pop_back();
    if (u.kind == 0 && u.index < (int)gLvl.L.meshes.size()) { if (gLvl.L.meshes[u.index].hidden != u.hidden) LvlSetHidden(u.index, u.hidden); LvlSetMeshWorld(u.index, u.world); gLvl.sel = u.index; gLvl.selActor = -1; }
    else if (u.kind == 1 && u.index < (int)gLvl.actors.size()) { Rot r{ u.rot[0], u.rot[1], u.rot[2] }; LvlSetActor(u.index, u.loc, r); gLvl.selActor = u.index; gLvl.sel = -1; }
}
static bool LvlSelPivot(float p[3]) {
    if (gGhost.sel) { for (int k = 0; k < 3; k++) p[k] = (gGhost.hasBox ? (gGhost.bmn[k] + gGhost.bmx[k]) * 0.5f : 0.f) + gGhost.off[k]; return true; }
    if (gLvl.sel >= 0) { const LevelMesh& m = gLvl.L.meshes[gLvl.sel]; auto it = gLvl.main.meshes.find(m.mesh);
        if (it != gLvl.main.meshes.end() && it->second.ok) { float c[3]; for (int k = 0; k < 3; k++) c[k] = (it->second.bmin[k] + it->second.bmax[k]) * 0.5f;
            for (int k = 0; k < 3; k++) p[k] = c[0] * m.world.m[0][k] + c[1] * m.world.m[1][k] + c[2] * m.world.m[2][k] + m.world.m[3][k]; }
        else for (int k = 0; k < 3; k++) p[k] = m.world.m[3][k]; return true; }
    if (gLvl.selActor >= 0) { memcpy(p, gLvl.actors[gLvl.selActor].loc, 12); return true; }
    return false;
}
static float SnapF(float v, float s) { return s > 0 ? roundf(v / s) * s : v; }
// gizmo drag: delta in axis units (move), degrees (rotate) or factor (scale)
static void LvlApplyDrag(int axis, float amount) {
    float ax[3] = { 0, 0, 0 }; ax[axis] = 1; float piv[3]; if (!LvlSelPivot(piv)) return;
    if (gGhost.sel) { gGhost.off[axis] += amount; return; }   // imported room: translation only
    if (gLvl.sel >= 0) {
        M4 w = gLvl.L.meshes[gLvl.sel].world;
        if (gLvl.mode == 0) { for (int k = 0; k < 3; k++) w.m[3][k] += ax[k] * amount; }
        else if (gLvl.mode == 1) { Rot r; if (axis == 0) r.roll = (i32)(amount / 360.f * 65536); else if (axis == 1) r.pitch = (i32)(amount / 360.f * 65536); else r.yaw = (i32)(amount / 360.f * 65536);
            w = m4Mul(m4Mul(m4Mul(w, translationMatrix(-piv[0], -piv[1], -piv[2])), rotationMatrix(r)), translationMatrix(piv[0], piv[1], piv[2])); }
        else { float s[3] = { 1, 1, 1 }; s[axis] = std::max(0.01f, 1 + amount); w = m4Mul(scaleMatrix(s[0], s[1], s[2]), w); }
        LvlSetMeshWorld(gLvl.sel, w);
    } else if (gLvl.selActor >= 0) {
        LvlActor& a = gLvl.actors[gLvl.selActor]; float loc[3] = { a.loc[0], a.loc[1], a.loc[2] }; Rot r = a.rot;
        if (gLvl.mode == 0) for (int k = 0; k < 3; k++) loc[k] += ax[k] * amount;
        else if (gLvl.mode == 1) { i32 d = (i32)(amount / 360.f * 65536); if (axis == 0) r.roll += d; else if (axis == 1) r.pitch += d; else r.yaw += d; }
        LvlSetActor(gLvl.selActor, loc, r);
    }
}

// ------------------------------------------------------------------ drawing
static void LvlSetBlend(int b, bool tex) {
    gDev->SetRenderState(D3DRS_ALPHATESTENABLE, (b == 1 && tex) ? TRUE : FALSE); gDev->SetRenderState(D3DRS_ALPHAREF, 100); gDev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
    gDev->SetTextureStageState(0, D3DTSS_ALPHAOP, tex && b != 0 ? D3DTOP_SELECTARG1 : D3DTOP_SELECTARG2); gDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE); gDev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    bool blend = b >= 2; gDev->SetRenderState(D3DRS_ALPHABLENDENABLE, blend); gDev->SetRenderState(D3DRS_ZWRITEENABLE, blend ? FALSE : TRUE);
    if (b == 2) { gDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA); gDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA); }
    else if (b == 3) { gDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE); gDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE); }
    else if (b == 4) { gDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR); gDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO); }
}
static void LvlRender(int w, int h) {
    if (w < 8 || h < 8) return;
    if (gLvl.rt && (gLvl.rtW != w || gLvl.rtH != h)) LevelReleaseRT();
    if (!gLvl.rt) { if (FAILED(gDev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &gLvl.rt, NULL))) return;
        if (FAILED(gDev->CreateDepthStencilSurface(w, h, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &gLvl.ds, NULL))) gDev->CreateDepthStencilSurface(w, h, D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, TRUE, &gLvl.ds, NULL); gLvl.rtW = w; gLvl.rtH = h; }
    gLvl.vw = w; gLvl.vh = h; float d[3]; LvlDir(d); float at[3] = { gLvl.eye[0] + d[0], gLvl.eye[1] + d[1], gLvl.eye[2] + d[2] }, up[3] = { 0, 0, 1 };
    gLvl.view = LookAtLH(gLvl.eye, at, up); gLvl.proj = PerspectiveLH(1.0f, (float)w / h, 8.f, 200000.f);
    IDirect3DSurface9 *oldRT = nullptr, *oldDS = nullptr, *surf = nullptr; gDev->GetRenderTarget(0, &oldRT); gDev->GetDepthStencilSurface(&oldDS);
    gLvl.rt->GetSurfaceLevel(0, &surf); gDev->SetRenderTarget(0, surf); gDev->SetDepthStencilSurface(gLvl.ds);
    D3DVIEWPORT9 vp = { 0, 0, (DWORD)w, (DWORD)h, 0, 1 }; gDev->SetViewport(&vp);
    gDev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(40, 44, 54, 255), 1.0f, 0);
    if (SUCCEEDED(gDev->BeginScene())) {
        gDev->SetTransform(D3DTS_VIEW, (D3DMATRIX*)&gLvl.view); gDev->SetTransform(D3DTS_PROJECTION, (D3DMATRIX*)&gLvl.proj);
        gDev->SetRenderState(D3DRS_ZENABLE, TRUE); gDev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE); gDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE); gDev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        gDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE); gDev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        gDev->SetRenderState(D3DRS_LIGHTING, TRUE); gDev->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE); gDev->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_RGBA(95, 95, 100, 255));
        D3DLIGHT9 L = {}; L.Type = D3DLIGHT_DIRECTIONAL; L.Diffuse.r = L.Diffuse.g = L.Diffuse.b = 0.8f; L.Direction.x = 0.4f; L.Direction.y = 0.3f; L.Direction.z = -0.85f; gDev->SetLight(0, &L); gDev->LightEnable(0, TRUE);
        D3DMATERIAL9 M = {}; M.Diffuse.r = M.Diffuse.g = M.Diffuse.b = M.Diffuse.a = 1; M.Ambient = M.Diffuse; gDev->SetMaterial(&M);
        gDev->SetFVF(GPUVERT_FVF);
        gDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR); gDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR); gDev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        gDev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP); gDev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
        gDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE); gDev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE); gDev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        int pass = 0; LvlCache* cur = &gLvl.main; const float* goff = nullptr; M4 gM = GhostMatrix();
        auto drawInst = [&](const LevelMesh& m, bool tex) { auto it = cur->meshes.find(m.mesh); if (it == cur->meshes.end() || !it->second.ok) return; const LvlGpuMesh& g = it->second;
            M4 w = goff ? m4Mul(m.world, gM) : m.world;
            gDev->SetTransform(D3DTS_WORLD, (D3DMATRIX*)&w); gDev->SetStreamSource(0, g.vb, 0, sizeof(GpuVert)); gDev->SetIndices(g.ib);
            for (auto& s : g.secs) { if (pass == 0 && s.blend >= 2 && tex) continue; if (pass == 2 && s.blend < 2) continue;
                IDirect3DTexture9* t = tex ? s.tex : nullptr; if (pass != 1) { gDev->SetTexture(0, t); gDev->SetTextureStageState(0, D3DTSS_COLOROP, t ? D3DTOP_MODULATE : D3DTOP_SELECTARG2); LvlSetBlend(tex ? s.blend : 0, t != nullptr); }
                if (s.tris) gDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, g.numVerts, s.first, s.tris); } };
        auto drawFlat = [&](std::vector<LvlGpuMesh>& v, bool terrain) { M4 I = goff ? gM : m4Identity(); gDev->SetTransform(D3DTS_WORLD, (D3DMATRIX*)&I);
            D3DMATERIAL9 tm = {}; if (terrain) { tm.Diffuse.r = 0.42f; tm.Diffuse.g = 0.45f; tm.Diffuse.b = 0.36f; tm.Diffuse.a = 1; tm.Ambient = tm.Diffuse; gDev->SetMaterial(&tm); }
            for (auto& g : v) { gDev->SetStreamSource(0, g.vb, 0, sizeof(GpuVert)); gDev->SetIndices(g.ib);
                for (auto& s : g.secs) { IDirect3DTexture9* t = gLvl.textured ? s.tex : nullptr; gDev->SetTexture(0, t); gDev->SetTextureStageState(0, D3DTSS_COLOROP, t ? D3DTOP_MODULATE : D3DTOP_SELECTARG2); LvlSetBlend(s.blend >= 2 ? 0 : s.blend, t != nullptr);
                    if (s.tris) gDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, g.numVerts, s.first, s.tris); } }
            if (terrain) { D3DMATERIAL9 M = {}; M.Diffuse.r = M.Diffuse.g = M.Diffuse.b = M.Diffuse.a = 1; M.Ambient = M.Diffuse; gDev->SetMaterial(&M); } };
        auto drawTrees = [&](const std::vector<M4>& trees) { if (!gLvl.showTrees || trees.empty()) return; LvlBuildTree(); if (!gTreeTrunk.ok) return;
            gDev->SetTexture(0, nullptr); gDev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2); LvlSetBlend(0, false);
            for (int part = 0; part < 2; part++) { LvlGpuMesh& g = part ? gTreeTop : gTreeTrunk; D3DMATERIAL9 tm = {}; tm.Diffuse.r = part ? 0.22f : 0.35f; tm.Diffuse.g = part ? 0.38f : 0.26f; tm.Diffuse.b = part ? 0.2f : 0.18f; tm.Diffuse.a = 1; tm.Ambient = tm.Diffuse; gDev->SetMaterial(&tm);
                gDev->SetStreamSource(0, g.vb, 0, sizeof(GpuVert)); gDev->SetIndices(g.ib);
                for (auto& w : trees) { gDev->SetTransform(D3DTS_WORLD, (D3DMATRIX*)&w); gDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, g.numVerts, 0, (UINT)g.idx.size() / 3); } }
            D3DMATERIAL9 M = {}; M.Diffuse.r = M.Diffuse.g = M.Diffuse.b = M.Diffuse.a = 1; M.Ambient = M.Diffuse; gDev->SetMaterial(&M); };
        auto drawLayer = [&](LvlCache& c, const Level& L) { cur = &c;
            if (gLvl.showBsp) drawFlat(c.bsp, false);
            if (gLvl.showTerrain) drawFlat(c.terrain, true);
            pass = 0; for (auto& m : L.meshes) { if (m.hidden) continue; drawInst(m, gLvl.textured); }
            if (gLvl.textured) { pass = 2; for (auto& m : L.meshes) { if (m.hidden) continue; drawInst(m, true); } }
            if (!goff) drawTrees(L.trees); };
        if (gCtx.enabled) for (auto& lp : gCtx.layers) if (lp->show && lp->loaded && !LvlIsCurrent(*lp)) drawLayer(lp->c, lp->L);
        drawLayer(gLvl.main, gLvl.L); cur = &gLvl.main;
        if (gGhost.on) { goff = gGhost.off; for (auto& gl : gGhost.layers) drawLayer(*gl.c, *gl.L); goff = nullptr; cur = &gLvl.main; }
        LvlSetBlend(0, false); pass = 1;
        // hidden and selected: wireframe
        gDev->SetRenderState(D3DRS_LIGHTING, FALSE); gDev->SetTexture(0, nullptr); gDev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1); gDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
        gDev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
        if (gLvl.showHidden) { gDev->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_RGBA(200, 90, 90, 255)); for (auto& m : gLvl.L.meshes) if (m.hidden) drawInst(m, false); }
        if (gLvl.sel >= 0 && gLvl.sel < (int)gLvl.L.meshes.size()) { gDev->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_RGBA(255, 210, 60, 255)); gDev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL); drawInst(gLvl.L.meshes[gLvl.sel], false); }
        gDev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID); gDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        gDev->EndScene();
    }
    gDev->SetRenderTarget(0, oldRT); gDev->SetDepthStencilSurface(oldDS); if (surf) surf->Release(); if (oldRT) oldRT->Release(); if (oldDS) oldDS->Release();
}

// ------------------------------------------------------------------ UI
static void LvlExportFbx() {
    wchar_t file[MAX_PATH] = L"level.fbx";
    OPENFILENAMEW of = {}; of.lStructSize = sizeof(of); of.hwndOwner = gHwnd; of.lpstrFilter = L"FBX (*.fbx)\0*.fbx\0\0"; of.lpstrFile = file; of.nMaxFile = MAX_PATH;
    of.lpstrDefExt = L"fbx"; of.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&of)) return;
    try {
        std::wstring outPath = file; std::wstring outDir = outPath.substr(0, outPath.find_last_of(L"\\/") + 1);
        if (gCtx.layers.empty() || gCtx.builtFor != gDoc.path) LvlBuildFamily(0);
        std::vector<LevelFbxPart> parts; std::map<const Package*, std::wstring> pathOf;
        std::vector<std::unique_ptr<Doc>> keepDoc; std::vector<std::unique_ptr<Level>> keepLvl; int nParts = 1;
        parts.push_back({ &gDoc.pk, &gLvl.L }); pathOf[&gDoc.pk] = gDoc.path;
        for (auto& lp : gCtx.layers) { LvlCtxLayer& l = *lp;
            if (!l.show || l.failed || l.path.empty() || LvlIsCurrent(l)) continue;
            if (l.doc && l.loaded) { parts.push_back({ &l.doc->pk, &l.L }); pathOf[&l.doc->pk] = l.path; nParts++; continue; }
            auto d = std::make_unique<Doc>(); if (!d->open(l.path).empty()) continue;
            auto lv = std::make_unique<Level>(parseLevel(d->pk));
            parts.push_back({ &d->pk, lv.get() }); pathOf[&d->pk] = l.path; keepDoc.push_back(std::move(d)); keepLvl.push_back(std::move(lv)); nParts++; }
        auto texFn = [&](const Package& pk, const std::vector<std::pair<std::string, i32>>& mats) {
            auto wf = [&](const std::string& name, const std::vector<u8>& b) { return WriteBytes(outDir + W(name), b.data(), b.size()); };
            return writeMaterialTextures(pk, mats, cookedDirOf(U8(pathOf[&pk])), ReadFileRange, wf, 2048); };
        LevelFbxReport rep; LevelFbxOptions opt; std::vector<u8> bytes = exportLevelFBX(parts, opt, rep, texFn);
        FILE* f = _wfopen(file, L"wb"); if (!f) { Status("cannot write the file"); return; }
        fwrite(bytes.data(), 1, bytes.size(), f); fclose(f);
        char msg[300]; snprintf(msg, sizeof msg, "FBX: %d sub-levels, %d instances, %d unique meshes, %d BSP, %.1f MB (skipped: %d skeletal, %d hidden, %d failed)", nParts, rep.instances, rep.uniqueMeshes, rep.bspModels, bytes.size() / 1048576.0, rep.skippedSkeletal, rep.skippedHidden, rep.failedMeshes);
        Status(msg);
    } catch (std::exception& e) { Status(e.what()); }
}static void LevelSidePanel() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); ImGui::TextWrapped("%s", T(L_LVL_HINT)); ImGui::PopStyleColor();
    if (ImGui::Button("Export level to FBX")) LvlExportFbx(); ImGui::SameLine(); if (ImGui::Button(T(L_RI_BTN))) RimpOpen();
    ImGui::SameLine(); if (ImGui::Button(T(L_LB_BTN))) LibOpen();
    ImGui::SameLine(); { bool co = CollIsOn(); if (ImGui::Checkbox(T(L_CO_MODE), &co)) CollSetOn(co); }
    if (CollIsOn()) { ImGui::Separator(); CollPanel(); return; }
    ImGui::Separator(); HolePanel(); ImGui::Separator();
    ImGui::RadioButton(T(L_LVL_MOVE), &gLvl.mode, 0); ImGui::SameLine(); ImGui::RadioButton(T(L_LVL_ROTATE), &gLvl.mode, 1); ImGui::SameLine(); ImGui::RadioButton(T(L_LVL_SCALE), &gLvl.mode, 2);
    ImGui::Checkbox(T(L_LVL_SNAP), &gLvl.snap); if (gLvl.snap) { ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::DragFloat("##sm", &gLvl.snapMove, 1, 1, 512, "%.0f u"); ImGui::SameLine(); ImGui::SetNextItemWidth(60); ImGui::DragFloat("##sr", &gLvl.snapRot, 1, 1, 90, "%.0f°"); }
    ImGui::Checkbox(T(L_TEXTURED), &gLvl.textured); ImGui::SameLine(); ImGui::Checkbox(T(L_LVL_SHOWHIDDEN), &gLvl.showHidden); ImGui::SameLine(); ImGui::Checkbox(T(L_LVL_ACTORS), &gLvl.showActors);  ImGui::Checkbox("BSP", &gLvl.showBsp);
    { int q = gLvl.texMax <= 128 ? 0 : gLvl.texMax <= 256 ? 1 : 2; ImGui::SetNextItemWidth(90); if (ImGui::Combo(T(L_LVL_TEXQ), &q, "128\0" "256\0" "512\0")) { gLvl.texMax = 128 << q; gLvl.gen = -1; } }
    ImGui::SameLine(); ImGui::Checkbox(T(L_LVL_TERRAIN), &gLvl.showTerrain); ImGui::SameLine(); ImGui::Checkbox(T(L_LVL_TREES), &gLvl.showTrees);
    // ---- context sub-levels
    if (ImGui::CollapsingHeader(T(L_LVL_CTX), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox(T(L_LVL_CTX_SHOW), &gCtx.enabled);
        if (!gCtx.parents.empty()) { ImGui::SameLine(); ImGui::SetNextItemWidth(160);
            if (ImGui::BeginCombo("##fam", gCtx.family.c_str())) { for (int i = 0; i < (int)gCtx.parents.size(); i++) if (ImGui::Selectable(gCtx.parents[i].c_str(), gCtx.parents[i] == gCtx.family)) { gCtx.up = i; LvlBuildFamily(i); } ImGui::EndCombo(); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T(L_LVL_CTX_FAMTIP)); }
        if (!gCtx.status.empty()) ImGui::TextDisabled("%s", gCtx.status.c_str());
        if (ImGui::SmallButton(T(L_LVL_CTX_ALL))) for (auto& l : gCtx.layers) if (!l->failed) l->show = true;
        ImGui::SameLine(); if (ImGui::SmallButton(T(L_LVL_CTX_NONE))) for (auto& l : gCtx.layers) l->show = false;
        ImGui::BeginChild("ctxlist", ImVec2(0, std::min(220.f, 24.f * (float)gCtx.layers.size() + 8)), ImGuiChildFlags_Borders);
        for (size_t i = 0; i < gCtx.layers.size(); i++) { LvlCtxLayer& l = *gCtx.layers[i]; ImGui::PushID((int)i); bool cur = LvlIsCurrent(l);
            ImGui::BeginDisabled(l.failed || cur); ImGui::Checkbox("##s", &l.show); ImGui::EndDisabled(); ImGui::SameLine();
            if (cur) ImGui::TextColored(ImVec4(1, .8f, .3f, 1), "%s  (%s)", l.name.c_str(), T(L_LVL_CTX_EDITING));
            else { ImGui::TextUnformatted(l.name.c_str());
                if (l.failed) { ImGui::SameLine(); ImGui::TextDisabled("(%s)", l.err.c_str()); }
                else if (l.show && l.loaded) { ImGui::SameLine(); ImGui::TextDisabled("%zu/%zu", l.c.loaded, l.c.toLoad.size()); }
                if (!l.failed) { ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 60); if (ImGui::SmallButton(T(L_LVL_CTX_EDIT))) { std::wstring p = l.path; ImGui::PopID(); ImGui::EndChild(); RequestAction(1, p); return; } } }
            ImGui::PopID(); }
        ImGui::EndChild();
    }
    ImGui::Separator();
    // selection
    if (gGhost.sel) { ImGui::TextColored(ImVec4(1, .8f, .3f, 1), "%s", T(L_RI_SELHINT)); ImGui::DragFloat3(T(L_RI_DELTA), gGhost.off, 1.f, 0, 0, "%.0f");
        if (ImGui::Button("<##gy")) gGhost.yaw90 = (gGhost.yaw90 + 3) & 3; ImGui::SameLine(); if (ImGui::Button(">##gy")) gGhost.yaw90 = (gGhost.yaw90 + 1) & 3; ImGui::SameLine(); ImGui::Text("%s: %d°", T(L_RI_ROT), (gGhost.yaw90 & 3) * 90); if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T(L_RI_ROT_TIP)); }
    else if (gLvl.sel >= 0 && gLvl.sel < (int)gLvl.L.meshes.size()) {
        LevelMesh& m = gLvl.L.meshes[gLvl.sel];
        ImGui::TextWrapped("%s", gDoc.pk.objPath(m.comp).c_str());
        ImGui::TextDisabled("%s: %s", T(L_LVL_MESH), m.mesh ? gDoc.pk.objPath(m.mesh).c_str() : "None");
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); ImGui::TextWrapped("%s", m.src == 0 ? T(L_LVL_SRC_COLL) : T(L_LVL_SRC_ACTOR)); ImGui::PopStyleColor();
        float loc[3], sc[3]; Rot r; decompose(m.world, loc, r, sc); float deg[3] = { r.pitch * 360.f / 65536, r.yaw * 360.f / 65536, r.roll * 360.f / 65536 };
        bool ch = false; static bool active = false;
        ch |= ImGui::DragFloat3(T(L_LVL_LOC), loc, 1.f, 0, 0, "%.1f"); bool a1 = ImGui::IsItemActivated();
        ch |= ImGui::DragFloat3(T(L_LVL_ROT), deg, 0.5f, 0, 0, "%.1f°"); bool a2 = ImGui::IsItemActivated();
        ch |= ImGui::DragFloat3(T(L_LVL_SCL), sc, 0.005f, 0.01f, 100.f, "%.3f"); bool a3 = ImGui::IsItemActivated();
        if (a1 || a2 || a3) LvlPushUndo(); (void)active;
        if (ch) { Rot nr{ (i32)lroundf(deg[0] / 360.f * 65536), (i32)lroundf(deg[1] / 360.f * 65536), (i32)lroundf(deg[2] / 360.f * 65536) }; for (auto& s : sc) if (fabsf(s) < 0.01f) s = 0.01f; LvlSetMeshWorld(gLvl.sel, compose(loc, nr, sc)); }
        if (ImGui::Button(T(L_LVL_RESET))) { LvlPushUndo(); LvlSetMeshWorld(gLvl.sel, gLvl.orig[gLvl.sel]); } ImGui::SameLine();
        if (ImGui::Button(m.hidden ? T(L_LVL_UNHIDE) : T(L_LVL_HIDE))) { LvlPushUndo(); LvlSetHidden(gLvl.sel, !m.hidden); } ImGui::SameLine();
        if (!m.hidden) { bool nc = LvlMeshNoCollision(gLvl.sel); if (ImGui::Button(nc ? T(L_CO_MESHCOLL) : T(L_CO_MESHNOCOLL))) LvlSetNoCollision(gLvl.sel, !nc); ImGui::SameLine(); }
        if (ImGui::Button(T(L_GOTO))) { gLevelMode = false; GotoObject(m.comp); }
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); ImGui::TextWrapped("%s", T(L_LVL_NOTE_COLL)); ImGui::PopStyleColor();
    } else if (gLvl.selActor >= 0 && gLvl.selActor < (int)gLvl.actors.size()) {
        LvlActor& a = gLvl.actors[gLvl.selActor];
        ImGui::TextWrapped("%s", gDoc.pk.objPath(a.idx).c_str()); ImGui::TextDisabled("%s", a.cls.c_str());
        float loc[3] = { a.loc[0], a.loc[1], a.loc[2] }; float deg[3] = { a.rot.pitch * 360.f / 65536, a.rot.yaw * 360.f / 65536, a.rot.roll * 360.f / 65536 };
        bool ch = ImGui::DragFloat3(T(L_LVL_LOC), loc, 1.f, 0, 0, "%.1f"); bool a1 = ImGui::IsItemActivated();
        ch |= ImGui::DragFloat3(T(L_LVL_ROT), deg, 0.5f, 0, 0, "%.1f°"); bool a2 = ImGui::IsItemActivated();
        if (a1 || a2) LvlPushUndo();
        if (ch) { Rot nr{ (i32)lroundf(deg[0] / 360.f * 65536), (i32)lroundf(deg[1] / 360.f * 65536), (i32)lroundf(deg[2] / 360.f * 65536) }; LvlSetActor(gLvl.selActor, loc, nr); }
        if (ImGui::Button(T(L_GOTO))) { gLevelMode = false; GotoObject(a.idx); }
    } else ImGui::TextDisabled("%s", T(L_LVL_NOSEL));
    ImGui::Separator();
    ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputTextWithHint("##lf", T(L_SEARCH), gLvl.filter, sizeof gLvl.filter);
    ImGui::BeginChild("lvllist", ImVec2(0, 0), ImGuiChildFlags_Borders);
    std::vector<int> rows; for (size_t i = 0; i < gLvl.L.meshes.size(); i++) { if (gLvl.filter[0]) { std::string s = gDoc.pk.objName(gLvl.L.meshes[i].mesh) + " " + gDoc.pk.objName(gLvl.L.meshes[i].comp); if (!icontains(s, gLvl.filter)) continue; } rows.push_back((int)i); }
    ImGuiListClipper clip; clip.Begin((int)rows.size());
    while (clip.Step()) for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) { int i = rows[r]; const LevelMesh& m = gLvl.L.meshes[i];
        char lbl[256]; snprintf(lbl, sizeof lbl, "%s%s##m%d", gDoc.pk.objName(m.mesh).c_str(), m.hidden ? "  (hidden)" : "", i);
        if (ImGui::Selectable(lbl, i == gLvl.sel)) { gLvl.sel = i; gLvl.selActor = -1; }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) { float p[3]; if (LvlSelPivot(p)) { float d[3]; LvlDir(d); for (int k = 0; k < 3; k++) gLvl.eye[k] = p[k] - d[k] * 600; } } }
    ImGui::EndChild();
}
static void LevelView() {
    if (gLvl.gen != gDocGen) { int keepSel = gLvl.sel, keepA = gLvl.selActor; bool same = gLvl.gen >= 0; LevelLoad(); if (same) { if (keepSel < (int)gLvl.L.meshes.size()) gLvl.sel = keepSel; if (keepA < (int)gLvl.actors.size()) gLvl.selActor = keepA; } }
    // loads meshes a few at a time (keeps the window responsive)
    double t0 = ImGui::GetTime(); while (gLvl.main.loaded < gLvl.main.toLoad.size() && ImGui::GetTime() - t0 < 0.05) { LvlBuildMesh(gLvl.main, gLvl.main.toLoad[gLvl.main.loaded]); gLvl.main.loaded++; }
    if (gCtx.enabled) { bool inFamily = false; for (auto& l : gCtx.layers) if (LvlIsCurrent(*l)) inFamily = true;
        if (!inFamily && gCtx.builtFor != gDoc.path) LvlBuildFamily(gCtx.up);
        if (gLvl.main.loaded >= gLvl.main.toLoad.size()) LvlCtxStep(0.04); }
    ImGuiIO& io = ImGui::GetIO();
    ImGui::BeginChild("lvlside", ImVec2(360, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX); LevelSidePanel(); ImGui::EndChild(); ImGui::SameLine();
    ImGui::BeginChild("lvlview", ImVec2(0, 0));
    ImGui::Text("%s  |  %zu %s, %zu %s, %zu %s", U8(gDoc.path.substr(gDoc.path.find_last_of(L"\\/") + 1)).c_str(), gLvl.L.meshes.size(), T(L_LVL_INSTANCES), gLvl.main.meshes.size(), T(L_LVL_MESHES), gLvl.actors.size(), T(L_LVL_ACTORSN));
    if (gLvl.main.loaded < gLvl.main.toLoad.size()) { ImGui::SameLine(); ImGui::TextColored(ImVec4(1, .8f, .3f, 1), "  %s %zu/%zu", T(L_LVL_LOADING), gLvl.main.loaded, gLvl.main.toLoad.size()); }
    if (!gLvl.L.err.empty()) ImGui::TextColored(ImVec4(1, .45f, .45f, 1), "%s", gLvl.L.err.c_str());
    ImVec2 av = ImGui::GetContentRegionAvail(); int w = (int)std::max(16.f, av.x), h = (int)std::max(16.f, av.y);
    LvlRender(w, h);
    ImVec2 org = ImGui::GetCursorScreenPos();
    if (gLvl.rt) ImGui::Image((ImTextureID)(intptr_t)gLvl.rt, ImVec2((float)w, (float)h)); else ImGui::Dummy(ImVec2((float)w, (float)h));
    bool hovered = ImGui::IsItemHovered(); ImDrawList* dl = ImGui::GetWindowDrawList();
    float mx = io.MousePos.x - org.x, my = io.MousePos.y - org.y;
    // actors as markers
    if (gLvl.showActors) for (size_t a = 0; a < gLvl.actors.size(); a++) { float sx, sy; if (!LvlProject(gLvl.actors[a].loc, sx, sy) || sx < 0 || sy < 0 || sx > w || sy > h) continue;
        ImU32 c = (int)a == gLvl.selActor ? IM_COL32(255, 210, 60, 255) : IM_COL32(120, 200, 255, 200); dl->AddCircle(ImVec2(org.x + sx, org.y + sy), 4, c, 8, 1.5f);
        if ((int)a == gLvl.selActor || (hovered && (sx - mx) * (sx - mx) + (sy - my) * (sy - my) < 64)) dl->AddText(ImVec2(org.x + sx + 6, org.y + sy - 7), c, gDoc.pk.objName(gLvl.actors[a].idx).c_str()); }
    // box of the imported room (yellow)
    if (gGhost.on && gGhost.hasBox && gGhost.boxOn) { float c[8][3]; M4 G = GhostMatrix(); for (int i = 0; i < 8; i++) { float q[3]; for (int k = 0; k < 3; k++) q[k] = (i >> k) & 1 ? gGhost.bmx[k] : gGhost.bmn[k]; for (int k = 0; k < 3; k++) c[i][k] = q[0] * G.m[0][k] + q[1] * G.m[1][k] + q[2] * G.m[2][k] + G.m[3][k]; }
        for (int i = 0; i < 8; i++) for (int k = 0; k < 3; k++) { int j = i | (1 << k); if (j == i) continue; float a[2], b[2]; if (!LvlProject(c[i], a[0], a[1]) || !LvlProject(c[j], b[0], b[1])) continue;
            dl->AddLine(ImVec2(org.x + a[0], org.y + a[1]), ImVec2(org.x + b[0], org.y + b[1]), gGhost.sel ? IM_COL32(255, 220, 60, 255) : IM_COL32(255, 200, 60, 150), gGhost.sel ? 2.f : 1.f); } }
    if (gGhost.sel) gLvl.mode = 0;
    CollOverlay(dl, org); HoleOverlay(dl, org);
    // gizmo
    float piv[3]; int hoverAxis = -1; float axScreen[3][2] = {}; float axLen = 0;
    if (LvlSelPivot(piv)) { float dd[3] = { piv[0] - gLvl.eye[0], piv[1] - gLvl.eye[1], piv[2] - gLvl.eye[2] }; axLen = std::max(10.f, sqrtf(dd[0] * dd[0] + dd[1] * dd[1] + dd[2] * dd[2]) * 0.15f);
        float ps[2]; if (LvlProject(piv, ps[0], ps[1])) {
            const ImU32 cols[3] = { IM_COL32(235, 70, 70, 255), IM_COL32(80, 220, 80, 255), IM_COL32(80, 140, 255, 255) };
            for (int a = 0; a < 3; a++) { float e[3] = { piv[0], piv[1], piv[2] }; e[a] += axLen; float es[2]; if (!LvlProject(e, es[0], es[1])) continue;
                axScreen[a][0] = es[0] - ps[0]; axScreen[a][1] = es[1] - ps[1];
                float vx = es[0] - ps[0], vy = es[1] - ps[1], l2 = vx * vx + vy * vy; float t = l2 > 0 ? ((mx - ps[0]) * vx + (my - ps[1]) * vy) / l2 : 0; t = std::max(0.f, std::min(1.f, t));
                float qx = ps[0] + vx * t - mx, qy = ps[1] + vy * t - my; if (hovered && qx * qx + qy * qy < 49 && gLvl.dragAxis < 0) hoverAxis = a;
                bool hi = hoverAxis == a || gLvl.dragAxis == a; dl->AddLine(ImVec2(org.x + ps[0], org.y + ps[1]), ImVec2(org.x + es[0], org.y + es[1]), hi ? IM_COL32(255, 255, 255, 255) : cols[a], hi ? 4.f : 2.5f);
                const char* lab[3] = { "X", "Y", "Z" }; dl->AddText(ImVec2(org.x + es[0] + 3, org.y + es[1] - 7), cols[a], lab[a]);
                if (gLvl.mode == 2) dl->AddRectFilled(ImVec2(org.x + es[0] - 4, org.y + es[1] - 4), ImVec2(org.x + es[0] + 4, org.y + es[1] + 4), cols[a]);
                else if (gLvl.mode == 1) dl->AddCircle(ImVec2(org.x + es[0], org.y + es[1]), 5, cols[a], 10, 2); } } }
    // input
    static float dragAccum = 0;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (hoverAxis >= 0 && !(gLvl.mode == 2 && gLvl.selActor >= 0)) { gLvl.dragAxis = hoverAxis; LvlPushUndo(); dragAccum = 0; }
        else if (CollIsOn()) CollPick(mx, my); else if (HolePlacing()) HolePick(mx, my); else LvlPick(mx, my);
    }
    if (gLvl.dragAxis >= 0) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) gLvl.dragAxis = -1;
        else if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0) {
            float* s = axScreen[gLvl.dragAxis]; float sl = sqrtf(s[0] * s[0] + s[1] * s[1]);
            float along = sl > 1 ? (io.MouseDelta.x * s[0] + io.MouseDelta.y * s[1]) / sl : io.MouseDelta.x;   // pixels along the axis
            float amount = gLvl.mode == 0 ? along * axLen / std::max(sl, 1.f) : gLvl.mode == 1 ? along * 0.5f : along * 0.005f;
            if (gLvl.snap && gLvl.mode != 2) { dragAccum += amount; float step = gLvl.mode == 0 ? gLvl.snapMove : gLvl.snapRot; float q = SnapF(dragAccum, step); if (q != 0) { LvlApplyDrag(gLvl.dragAxis, q); dragAccum -= q; } }
            else LvlApplyDrag(gLvl.dragAxis, amount);
        }
    }
    if (hovered || ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && (hovered || ImGui::IsMouseDragging(ImGuiMouseButton_Right))) {
            gLvl.yaw += io.MouseDelta.x * 0.005f; gLvl.pitch = std::max(-1.55f, std::min(1.55f, gLvl.pitch - io.MouseDelta.y * 0.005f));
            float d[3]; LvlDir(d); float right[3] = { -sinf(gLvl.yaw), cosf(gLvl.yaw), 0 }; float sp = gLvl.speed * io.DeltaTime * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 4.f : 1.f);
            for (int k = 0; k < 3; k++) { if (ImGui::IsKeyDown(ImGuiKey_W)) gLvl.eye[k] += d[k] * sp; if (ImGui::IsKeyDown(ImGuiKey_S)) gLvl.eye[k] -= d[k] * sp;
                if (ImGui::IsKeyDown(ImGuiKey_D)) gLvl.eye[k] += right[k] * sp; if (ImGui::IsKeyDown(ImGuiKey_A)) gLvl.eye[k] -= right[k] * sp; }
            if (ImGui::IsKeyDown(ImGuiKey_E)) gLvl.eye[2] += sp; if (ImGui::IsKeyDown(ImGuiKey_Q)) gLvl.eye[2] -= sp;
            if (io.MouseWheel != 0) gLvl.speed = std::max(50.f, std::min(20000.f, gLvl.speed * (io.MouseWheel > 0 ? 1.25f : 0.8f)));
        } else if (hovered && io.MouseWheel != 0) { float d[3]; LvlDir(d); for (int k = 0; k < 3; k++) gLvl.eye[k] += d[k] * io.MouseWheel * gLvl.speed * 0.25f; }
        if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) { float right[3] = { -sinf(gLvl.yaw), cosf(gLvl.yaw), 0 }; float k2 = gLvl.speed * 0.002f;
            for (int k = 0; k < 3; k++) gLvl.eye[k] -= right[k] * io.MouseDelta.x * k2; gLvl.eye[2] += io.MouseDelta.y * k2; }
    }
    if (hovered && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && !io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_F)) { float p[3]; if (LvlSelPivot(p)) { float d[3]; LvlDir(d); for (int k = 0; k < 3; k++) gLvl.eye[k] = p[k] - d[k] * 600; } }
        if (ImGui::IsKeyPressed(ImGuiKey_W)) gLvl.mode = 0; if (ImGui::IsKeyPressed(ImGuiKey_E)) gLvl.mode = 1; if (ImGui::IsKeyPressed(ImGuiKey_R)) gLvl.mode = 2;
        if (ImGui::IsKeyPressed(ImGuiKey_H) && gLvl.sel >= 0) { LvlPushUndo(); LvlSetHidden(gLvl.sel, !gLvl.L.meshes[gLvl.sel].hidden); }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { gLvl.sel = -1; gLvl.selActor = -1; gGhost.sel = false; }
    }
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) { if (CollIsOn()) CollUndoLast(); else LvlUndoLast(); }
    ImGui::EndChild();
}
