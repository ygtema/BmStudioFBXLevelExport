// bmlevelfbx.hpp - whole level (PersistentLevel static meshes + BSP) -> one BINARY FBX 7.4.
// Every unique StaticMesh is written once (Geometry); every instance is a Model with its own
// transform that points to the shared Geometry. BSP models (world space) are written as one mesh each.
// UE (left-handed) -> FBX: mirrors Y, flips winding (same convention as exportFBXStatic). Units: cm, Z up.
// Skipped: SkeletalMesh, Terrain, SpeedTree.
#pragma once
#include "bmfbx.hpp"
#include "bmlevel.hpp"
#include <functional>
#include <cmath>
#include <set>

namespace bm {

struct LevelFbxOptions { bool includeHidden = false; bool includeBsp = true; bool fixWinding = true; int lod = 0; };   // fixWinding: flips meshes whose triangle winding disagrees with their vertex normals
struct LevelFbxReport { int instances = 0, uniqueMeshes = 0, skippedSkeletal = 0, skippedHidden = 0, failedMeshes = 0, bspModels = 0, windingFixed = 0; size_t tris = 0, bspTris = 0; };
struct LevelFbxPart { const Package* pk; const Level* L; };   // one sub-level (its package + parseLevel result)
// {material path, material object index} -> {short material name -> png file (relative to the .fbx)}
using LevelTexFn = std::function<std::map<std::string, std::string>(const Package& pk, const std::vector<std::pair<std::string, i32>>& mats)>;

inline std::vector<u8> exportLevelFBX(const std::vector<LevelFbxPart>& parts, const LevelFbxOptions& opt, LevelFbxReport& rep, LevelTexFn texFn = nullptr) {
    using fbx::Node; using fbx::Mat;
    int64_t nextId = 1000000; auto newId = [&]() { return nextId++; };
    std::vector<Node> top;
    { Node h("FBXHeaderExtension"); h.child("FBXHeaderVersion").I(1003); h.child("FBXVersion").I(7400); h.child("EncryptionType").I(0);
      Node& ts = h.child("CreationTimeStamp"); ts.child("Version").I(1000); ts.child("Year").I(2026); ts.child("Month").I(1); ts.child("Day").I(1); ts.child("Hour").I(0); ts.child("Minute").I(0); ts.child("Second").I(0); ts.child("Millisecond").I(0);
      h.child("Creator").S("BmStudio (Batman: Arkham Asylum)"); top.push_back(h); }
    top.push_back(Node("FileId")); { std::vector<u8> id(16, 0x42); fbx::Prop p{ 'R', {} }; fbx::put32(p.d, 16); p.d.insert(p.d.end(), id.begin(), id.end()); top.back().props.push_back(p); }
    top.push_back(Node("CreationTime")); top.back().S("1970-01-01 10:00:00:000");
    top.push_back(Node("Creator")); top.back().S("BmStudio");
    { Node g("GlobalSettings"); g.child("Version").I(1000); Node& p = g.child("Properties70");
      fbx::P(p, "UpAxis", "int", "Integer", "", 2); fbx::P(p, "UpAxisSign", "int", "Integer", "", 1); fbx::P(p, "FrontAxis", "int", "Integer", "", 1); fbx::P(p, "FrontAxisSign", "int", "Integer", "", -1);
      fbx::P(p, "CoordAxis", "int", "Integer", "", 0); fbx::P(p, "CoordAxisSign", "int", "Integer", "", 1); fbx::P(p, "OriginalUpAxis", "int", "Integer", "", 2); fbx::P(p, "OriginalUpAxisSign", "int", "Integer", "", 1);
      fbx::Pd(p, "UnitScaleFactor", "double", "Number", "", 1.0); fbx::Pd(p, "OriginalUnitScaleFactor", "double", "Number", "", 1.0);
      fbx::P(p, "TimeMode", "enum", "", "", 6); fbx::Pl(p, "TimeSpanStart", "KTime", "Time", "", 0); fbx::Pl(p, "TimeSpanStop", "KTime", "Time", "", 46186158000LL); fbx::Pd(p, "CustomFrameRate", "double", "Number", "", -1);
      top.push_back(g); }
    { Node d("Documents"); d.child("Count").I(1); Node& doc = d.child("Document"); doc.L(newId()).S("").S("Scene"); Node& p = doc.child("Properties70"); fbx::Ps(p, "SourceObject", "object", "", "", ""); fbx::Ps(p, "ActiveAnimStackName", "KString", "", "", ""); doc.child("RootNode").L(0); top.push_back(d); }
    top.push_back(Node("References"));
    Node objects("Objects"); Node conns("Connections");
    auto C = [&](int64_t child, int64_t parent) { conns.child("C").S("OO").L(child).L(parent); };
    auto CP = [&](int64_t child, int64_t parent, const std::string& prop) { conns.child("C").S("OP").L(child).L(parent).S(prop); };
    int counts[16] = { 0 };   // 0 Model 1 Geometry 2 Material 3 NodeAttribute 4 Deformer 5 Pose 6 AnimStack 7 AnimLayer 8 AnimCurveNode 9 AnimCurve 10 Texture 11 Video

    struct MeshRec { bool ok = false; int64_t geo = 0; std::string name; std::vector<std::string> mats; std::map<std::string, std::string> tex; size_t tris = 0; };
    struct GeoSec { u32 first, tris; std::string mat; };
    std::map<std::pair<const Package*, i32>, MeshRec> recs; std::map<std::string, int64_t> matIds; std::set<std::string> texTried; int instNo = 0;

    // geometry node: P/UV per vertex, optional normals N, triangle sections; mirrors Y and flips the winding
    auto emitGeometry = [&](MeshRec& rec, const std::string& name, const std::vector<std::array<float, 3>>& P, const std::vector<std::array<float, 3>>* N,
                            const std::vector<std::array<float, 2>>& UV, const std::vector<uint32_t>& IDX, const std::vector<GeoSec>& secs) -> bool {
        Node g("Geometry"); int64_t geoId = newId(); g.L(geoId).S(fbx::nameClass(name, "Geometry")).S("Mesh");
        g.child("Properties70"); g.child("GeometryVersion").I(124);
        std::vector<double> v; v.reserve(P.size() * 3); for (auto& p : P) { v.push_back(p[0]); v.push_back(-p[1]); v.push_back(p[2]); }
        g.child("Vertices").Ad(v);
        std::vector<int32_t> pvi, uvIdx, matIdx; std::vector<double> nn; std::vector<std::string> mats;
        std::vector<int> secMat; for (auto& s : secs) { std::string n = fbx::shortName(s.mat); int f = -1; for (size_t u = 0; u < mats.size(); u++) if (mats[u] == n) f = (int)u; if (f < 0) { f = (int)mats.size(); mats.push_back(n); } secMat.push_back(f); }
        size_t tris = 0;
        bool flip = false;   // winding check: compare the geometric normal of every triangle (final space) with the stored vertex normals
        if (N && opt.fixWinding) { double acc = 0;
            for (auto& s : secs) for (u32 t = 0; t < s.tris; t++) { size_t b = s.first + (size_t)t * 3; if (b + 2 >= IDX.size()) break;
                uint32_t c0 = IDX[b], c1 = IDX[b + 1], c2 = IDX[b + 2]; if (c0 >= P.size() || c1 >= P.size() || c2 >= P.size()) continue;
                double a[3] = { P[c0][0], -(double)P[c0][1], P[c0][2] }, bb[3] = { P[c2][0], -(double)P[c2][1], P[c2][2] }, cc[3] = { P[c1][0], -(double)P[c1][1], P[c1][2] };   // final order (c0,c2,c1)
                double e1[3] = { bb[0] - a[0], bb[1] - a[1], bb[2] - a[2] }, e2[3] = { cc[0] - a[0], cc[1] - a[1], cc[2] - a[2] };
                double cr[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
                double nx = (*N)[c0][0] + (*N)[c1][0] + (*N)[c2][0], ny = -((double)(*N)[c0][1] + (*N)[c1][1] + (*N)[c2][1]), nz = (*N)[c0][2] + (*N)[c1][2] + (*N)[c2][2];
                acc += cr[0] * nx + cr[1] * ny + cr[2] * nz; }
            flip = acc < 0; }
        for (size_t si = 0; si < secs.size(); si++) { const auto& s = secs[si];
            for (u32 t = 0; t < s.tris; t++) { size_t b = s.first + (size_t)t * 3; if (b + 2 >= IDX.size()) break;
                uint32_t c[3] = { IDX[b], flip ? IDX[b + 1] : IDX[b + 2], flip ? IDX[b + 2] : IDX[b + 1] };
                bool bad = false; for (int k = 0; k < 3; k++) if (c[k] >= P.size()) bad = true; if (bad) continue;
                for (int k = 0; k < 3; k++) { int32_t vi = (int32_t)c[k]; pvi.push_back(k == 2 ? ~vi : vi); uvIdx.push_back(vi);
                    if (N) { const auto& n = (*N)[vi]; nn.push_back(n[0]); nn.push_back(-n[1]); nn.push_back(n[2]); } }
                matIdx.push_back((int32_t)secMat[si]); tris++; } }
        if (pvi.empty()) return false;
        counts[1]++;
        g.child("PolygonVertexIndex").Ai(pvi);
        if (N) { Node& ln = g.child("LayerElementNormal"); ln.I(0); ln.child("Version").I(101); ln.child("Name").S(""); ln.child("MappingInformationType").S("ByPolygonVertex"); ln.child("ReferenceInformationType").S("Direct"); ln.child("Normals").Ad(nn); }
        { Node& lu = g.child("LayerElementUV"); lu.I(0); lu.child("Version").I(101); lu.child("Name").S("UVMap"); lu.child("MappingInformationType").S("ByPolygonVertex"); lu.child("ReferenceInformationType").S("IndexToDirect");
          std::vector<double> uv; uv.reserve(UV.size() * 2); for (auto& p : UV) { uv.push_back(p[0]); uv.push_back(1.0 - p[1]); } lu.child("UV").Ad(uv); lu.child("UVIndex").Ai(uvIdx); }
        { Node& lm = g.child("LayerElementMaterial"); lm.I(0); lm.child("Version").I(101); lm.child("Name").S(""); lm.child("MappingInformationType").S("ByPolygon"); lm.child("ReferenceInformationType").S("IndexToDirect"); lm.child("Materials").Ai(matIdx); }
        { Node& ly = g.child("Layer"); ly.I(0); ly.child("Version").I(100);
          if (N) { Node& e = ly.child("LayerElement"); e.child("Type").S("LayerElementNormal"); e.child("TypedIndex").I(0); }
          for (const char* t : { "LayerElementMaterial", "LayerElementUV" }) { Node& e = ly.child("LayerElement"); e.child("Type").S(t); e.child("TypedIndex").I(0); } }
        objects.kids.push_back(std::move(g));
        if (flip) rep.windingFixed++;
        rec.name = name; rec.geo = geoId; rec.mats = mats; rec.tris = tris; rec.ok = true; return true;
    };

    // asks the texture callback only about materials that were not processed yet
    auto fetchTextures = [&](const Package& pk, MeshRec& rec, const std::vector<std::pair<std::string, i32>>& mats) {
        if (!texFn) return; std::vector<std::pair<std::string, i32>> need;
        for (auto& e : mats) { std::string n = fbx::shortName(e.first); if (texTried.insert(n).second) need.push_back(e); }
        if (!need.empty()) { try { rec.tex = texFn(pk, need); } catch (...) {} }
    };

    auto buildMesh = [&](const Package& pk, i32 meshIdx) -> MeshRec& {
        auto key = std::make_pair(&pk, meshIdx); auto it = recs.find(key); if (it != recs.end()) return it->second;
        MeshRec& rec = recs[key];
        StaticMesh st; try { st = parseStaticMesh(pk, meshIdx); } catch (...) { return rec; }
        if (st.lods.empty()) return rec;
        const StaticLOD& l = st.lods[std::min<size_t>((size_t)std::max(0, opt.lod), st.lods.size() - 1)];
        if (l.verts.empty() || l.indices.empty()) return rec;
        std::vector<std::array<float, 3>> P, N; std::vector<std::array<float, 2>> UV; P.reserve(l.verts.size()); N.reserve(l.verts.size()); UV.reserve(l.verts.size());
        for (auto& v : l.verts) { P.push_back({ v.pos[0], v.pos[1], v.pos[2] }); N.push_back({ v.normal[0], v.normal[1], v.normal[2] }); UV.push_back({ v.uv[0], v.uv[1] }); }
        std::vector<GeoSec> secs; std::vector<std::pair<std::string, i32>> mats;
        for (auto& e : l.elements) { secs.push_back({ e.firstIndex, e.numTris, e.materialPath }); mats.push_back({ e.materialPath, e.material }); }
        if (!emitGeometry(rec, fbx::shortName(pk.objName(meshIdx)), P, &N, UV, l.indices, secs)) return rec;
        fetchTextures(pk, rec, mats);
        rep.uniqueMeshes++; return rec;
    };

    auto getMaterial = [&](const std::string& n, const std::map<std::string, std::string>& tex) -> int64_t {
        auto it = matIds.find(n); if (it != matIds.end()) return it->second;
        int64_t id = newId(); matIds[n] = id;
        Node mt("Material"); mt.L(id).S(fbx::nameClass(n, "Material")).S(""); counts[2]++; mt.child("Version").I(102); mt.child("ShadingModel").S("phong"); mt.child("MultiLayer").I(0);
        { Node& p = mt.child("Properties70"); fbx::P3(p, "DiffuseColor", "Color", "", "A", 0.8, 0.8, 0.8); }
        objects.kids.push_back(std::move(mt));
        auto tf = tex.find(n);
        if (tf != tex.end() && !tf->second.empty()) {
            std::string tn = tf->second; size_t dot = tn.rfind('.'); std::string base = dot == std::string::npos ? tn : tn.substr(0, dot);
            int64_t tid = newId(), vid = newId();
            { Node t("Texture"); t.L(tid).S(fbx::nameClass(base, "Texture")).S(""); counts[10]++; t.child("Type").S("TextureVideoClip"); t.child("Version").I(202); t.child("TextureName").S(fbx::nameClass(base, "Texture"));
              { Node& tp = t.child("Properties70"); fbx::P(tp, "UseMaterial", "bool", "", "", 1); }
              t.child("Media").S(fbx::nameClass(base, "Video")); t.child("FileName").S(tn); t.child("RelativeFilename").S(tn);
              t.child("ModelUVTranslation").D(0).D(0); t.child("ModelUVScaling").D(1).D(1); t.child("Texture_Alpha_Source").S("None"); t.child("Cropping").I(0).I(0).I(0).I(0); objects.kids.push_back(std::move(t)); }
            { Node vd("Video"); vd.L(vid).S(fbx::nameClass(base, "Video")).S("Clip"); counts[11]++; vd.child("Type").S("Clip");
              { Node& vp = vd.child("Properties70"); fbx::Ps(vp, "Path", "KString", "XRefUrl", "", tn); }
              vd.child("UseMipMap").I(0); vd.child("Filename").S(tn); vd.child("RelativeFilename").S(tn); objects.kids.push_back(std::move(vd)); }
            C(vid, tid); CP(tid, id, "DiffuseColor");
        }
        return id;
    };

    // Model node for a geometry (T in cm, R euler XYZ degrees, S)
    auto addModel = [&](const MeshRec& rec, const std::string& nm, double tx, double ty, double tz, double ex, double ey, double ez, double sx, double sy, double sz) {
        int64_t mid = newId();
        Node mo("Model"); mo.L(mid).S(fbx::nameClass(nm, "Model")).S("Mesh"); counts[0]++; mo.child("Version").I(232);
        { Node& p = mo.child("Properties70"); fbx::P(p, "DefaultAttributeIndex", "int", "Integer", "", 0);
          fbx::P3(p, "Lcl Translation", "Lcl Translation", "", "A", tx, ty, tz); fbx::P3(p, "Lcl Rotation", "Lcl Rotation", "", "A", ex, ey, ez); fbx::P3(p, "Lcl Scaling", "Lcl Scaling", "", "A", sx, sy, sz); }
        mo.child("Shading").C(true); mo.child("Culling").S("CullingOff");
        objects.kids.push_back(std::move(mo));
        C(rec.geo, mid); C(mid, 0);
        for (auto& mn : rec.mats) C(getMaterial(mn, rec.tex), mid);   // connection order = material index
    };

    int partNo = 0;
    for (const LevelFbxPart& part : parts) {
        partNo++;
        for (const LevelMesh& m : part.L->meshes) {
            if (m.kind != 0) { rep.skippedSkeletal++; continue; }
            if (m.hidden && !opt.includeHidden) { rep.skippedHidden++; continue; }
            if (m.mesh <= 0) continue;
            MeshRec& rec = buildMesh(*part.pk, m.mesh);
            if (!rec.ok) { if (rec.geo == 0) rep.failedMeshes++; continue; }
            // world matrix -> T / R / S (column form, Y mirrored)
            Mat A = fbx::fromUE(m.world); double s[3];
            for (int j = 0; j < 3; j++) s[j] = std::sqrt(A.m[0][j] * A.m[0][j] + A.m[1][j] * A.m[1][j] + A.m[2][j] * A.m[2][j]);
            double det = A.m[0][0] * (A.m[1][1] * A.m[2][2] - A.m[1][2] * A.m[2][1]) - A.m[0][1] * (A.m[1][0] * A.m[2][2] - A.m[1][2] * A.m[2][0]) + A.m[0][2] * (A.m[1][0] * A.m[2][1] - A.m[1][1] * A.m[2][0]);
            if (det < 0) s[0] = -s[0];
            Mat R = fbx::ident(); for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) R.m[i][j] = s[j] != 0 ? A.m[i][j] / s[j] : (i == j ? 1.0 : 0.0);
            double ex, ey, ez; fbx::toEuler(R, ex, ey, ez);
            instNo++;
            addModel(rec, rec.name + "_" + std::to_string(instNo), A.m[0][3], A.m[1][3], A.m[2][3], ex, ey, ez,
                     std::fabs(s[0]) < 1e-9 ? 1 : s[0], std::fabs(s[1]) < 1e-9 ? 1 : s[1], std::fabs(s[2]) < 1e-9 ? 1 : s[2]);
            rep.instances++; rep.tris += rec.tris;
        }
        // BSP: level models are already in world space (same rule as the viewer: not on a beta room, brush/volume models excluded)
        if (opt.includeBsp && part.pk->findPath("BetaBSP") <= 0) {
            std::vector<i32> mdls; try { mdls = levelModels(*part.pk); } catch (...) {}
            int bn = 0;
            for (i32 mdl : mdls) {
                BspModel bm = parseModel(*part.pk, mdl); if (!bm.err.empty() || bm.verts.empty() || bm.idx.empty()) continue;
                std::vector<std::array<float, 3>> P; std::vector<std::array<float, 2>> UV; P.reserve(bm.verts.size()); UV.reserve(bm.verts.size());
                for (auto& v : bm.verts) { P.push_back({ v.pos[0], v.pos[1], v.pos[2] }); UV.push_back({ v.uv[0], v.uv[1] }); }
                std::vector<GeoSec> secs; std::vector<std::pair<std::string, i32>> mats;
                for (auto& s : bm.secs) { std::string mn = s.first > 0 ? fbx::shortName(part.pk->objName(s.first)) : std::string("BSP_NoMaterial");
                    secs.push_back({ s.second.first, s.second.second, mn }); if (s.first > 0) mats.push_back({ mn, s.first }); }
                MeshRec rec; std::string nm = "BSP_" + std::to_string(partNo) + "_" + std::to_string(++bn);
                if (!emitGeometry(rec, nm, P, nullptr, UV, bm.idx, secs)) continue;
                fetchTextures(*part.pk, rec, mats);
                addModel(rec, nm, 0, 0, 0, 0, 0, 0, 1, 1, 1);
                rep.bspModels++; rep.bspTris += rec.tris;
            }
        }
    }

    { Node d("Definitions"); d.child("Version").I(100); int total = 1; for (int i = 0; i < 12; i++) total += counts[i]; d.child("Count").I(total);
      { Node& ot = d.child("ObjectType"); ot.S("GlobalSettings"); ot.child("Count").I(1); }
      const char* names[12] = { "Model", "Geometry", "Material", "NodeAttribute", "Deformer", "Pose", "AnimationStack", "AnimationLayer", "AnimationCurveNode", "AnimationCurve", "Texture", "Video" };
      for (int i = 0; i < 12; i++) if (counts[i]) { Node& ot = d.child("ObjectType"); ot.S(names[i]); ot.child("Count").I(counts[i]); }
      top.push_back(d); }
    top.push_back(std::move(objects)); top.push_back(std::move(conns));
    { Node t("Takes"); t.child("Current").S(""); top.push_back(t); }
    return fbx::encode(top);
}

inline std::vector<u8> exportLevelFBX(const Package& pk, const Level& L, const LevelFbxOptions& opt, LevelFbxReport& rep) {
    std::vector<LevelFbxPart> parts; parts.push_back({ &pk, &L }); return exportLevelFBX(parts, opt, rep, nullptr);
}

} // namespace bm
