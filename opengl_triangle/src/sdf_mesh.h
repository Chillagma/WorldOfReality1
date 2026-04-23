#pragma once
#include <glad/glad.h>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

struct SdfVec3 {
    float x, y, z;

    SdfVec3() { x = 0; y = 0; z = 0; }
    SdfVec3(float ax, float ay, float az) { x = ax; y = ay; z = az; }

    SdfVec3 sub(SdfVec3 o) { return SdfVec3(x - o.x, y - o.y, z - o.z); }
    SdfVec3 add(SdfVec3 o) { return SdfVec3(x + o.x, y + o.y, z + o.z); }
    SdfVec3 scale(float s) { return SdfVec3(x * s, y * s, z * s); }
    float   dot3(SdfVec3 o) { return x * o.x + y * o.y + z * o.z; }
    float   len() { return sqrtf(x * x + y * y + z * z); }

    SdfVec3 cross3(SdfVec3 o) {
        return SdfVec3(
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        );
    }
};

static SdfVec3 sdfMin3(SdfVec3 a, SdfVec3 b) {
    float rx = a.x < b.x ? a.x : b.x;
    float ry = a.y < b.y ? a.y : b.y;
    float rz = a.z < b.z ? a.z : b.z;
    return SdfVec3(rx, ry, rz);
}

static SdfVec3 sdfMax3(SdfVec3 a, SdfVec3 b) {
    float rx = a.x > b.x ? a.x : b.x;
    float ry = a.y > b.y ? a.y : b.y;
    float rz = a.z > b.z ? a.z : b.z;
    return SdfVec3(rx, ry, rz);
}

static float sdfMaxF(float a, float b, float c) {
    float m = a;
    if (b > m) m = b;
    if (c > m) m = c;
    return m;
}

static float sdfPtTriDist(SdfVec3 pp, SdfVec3 aa, SdfVec3 bb, SdfVec3 cc) {
    SdfVec3 ab = bb.sub(aa);
    SdfVec3 ac = cc.sub(aa);
    SdfVec3 ap = pp.sub(aa);
    float d1 = ab.dot3(ap);
    float d2 = ac.dot3(ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return pp.sub(aa).len();

    SdfVec3 bp = pp.sub(bb);
    float d3 = ab.dot3(bp);
    float d4 = ac.dot3(bp);
    if (d3 >= 0.0f && d4 <= d3) return pp.sub(bb).len();

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return pp.sub(aa.add(ab.scale(v))).len();
    }

    SdfVec3 cp = pp.sub(cc);
    float d5 = ab.dot3(cp);
    float d6 = ac.dot3(cp);
    if (d6 >= 0.0f && d5 <= d6) return pp.sub(cc).len();

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return pp.sub(aa.add(ac.scale(w))).len();
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return pp.sub(bb.add(cc.sub(bb).scale(w))).len();
    }

    float dn = 1.0f / (va + vb + vc);
    return pp.sub(aa.add(ab.scale(vb * dn)).add(ac.scale(vc * dn))).len();
}

static bool sdfRayHit(SdfVec3 oo, SdfVec3 dd, SdfVec3 aa, SdfVec3 bb, SdfVec3 cc) {
    SdfVec3 e1 = bb.sub(aa);
    SdfVec3 e2 = cc.sub(aa);
    SdfVec3 hh = dd.cross3(e2);
    float det = e1.dot3(hh);
    if (det > -1e-7f && det < 1e-7f) return false;

    float inv = 1.0f / det;
    SdfVec3 ss = oo.sub(aa);
    float uu = inv * ss.dot3(hh);
    if (uu < 0.0f || uu > 1.0f) return false;

    SdfVec3 qq = ss.cross3(e1);
    float vv = inv * dd.dot3(qq);
    if (vv < 0.0f || uu + vv > 1.0f) return false;

    return inv * e2.dot3(qq) > 1e-7f;
}

struct SDFMesh {
    GLuint texID;
    int    gridRes;
    float  boundsMin[3];
    float  boundsExt[3];

    SDFMesh() {
        texID = 0;
        gridRes = 64;
        boundsMin[0] = 0; boundsMin[1] = 0; boundsMin[2] = 0;
        boundsExt[0] = 1; boundsExt[1] = 1; boundsExt[2] = 1;
    }

    bool load(const std::string& stlPath, int resolution) {
        gridRes = resolution;

        std::vector<SdfVec3> triA, triB, triC;
        SdfVec3 lo(1e20f, 1e20f, 1e20f);
        SdfVec3 hi(-1e20f, -1e20f, -1e20f);

        // ── Check if ASCII or binary ──
        std::ifstream testFile(stlPath);
        std::string firstWord;
        testFile >> firstWord;
        testFile.close();
        bool isAscii = (firstWord == "solid");

        if (isAscii) {
            std::ifstream f(stlPath);
            if (!f) { std::cerr << "[SDF] Can't open " << stlPath << "\n"; return false; }

            std::string line;
            SdfVec3 verts[3];
            int vertIdx = 0;

            while (std::getline(f, line)) {
                size_t start = line.find_first_not_of(" \t");
                if (start == std::string::npos) continue;
                line = line.substr(start);

                if (line.substr(0, 6) == "vertex") {
                    float x, y, z;
                    sscanf(line.c_str(), "vertex %f %f %f", &x, &y, &z);
                    verts[vertIdx++] = SdfVec3(x, y, z);
                    if (vertIdx == 3) {
                        triA.push_back(verts[0]);
                        triB.push_back(verts[1]);
                        triC.push_back(verts[2]);
                        lo = sdfMin3(lo, sdfMin3(verts[0], sdfMin3(verts[1], verts[2])));
                        hi = sdfMax3(hi, sdfMax3(verts[0], sdfMax3(verts[1], verts[2])));
                        vertIdx = 0;
                    }
                }
            }
        }
        else {
            std::ifstream f(stlPath, std::ios::binary);
            if (!f) { std::cerr << "[SDF] Can't open " << stlPath << "\n"; return false; }

            char hdr[80];
            f.read(hdr, 80);

            uint32_t triCount = 0;
            f.read((char*)&triCount, 4);
            if (triCount == 0) { std::cerr << "[SDF] Empty STL\n"; return false; }

            triA.resize(triCount);
            triB.resize(triCount);
            triC.resize(triCount);

            for (uint32_t i = 0; i < triCount; i++) {
                float raw[12];
                f.read((char*)raw, 48);
                uint16_t attr;
                f.read((char*)&attr, 2);

                triA[i] = SdfVec3(raw[3], raw[4], raw[5]);
                triB[i] = SdfVec3(raw[6], raw[7], raw[8]);
                triC[i] = SdfVec3(raw[9], raw[10], raw[11]);

                lo = sdfMin3(lo, sdfMin3(triA[i], sdfMin3(triB[i], triC[i])));
                hi = sdfMax3(hi, sdfMax3(triA[i], sdfMax3(triB[i], triC[i])));
            }
        }

        uint32_t triCount = (uint32_t)triA.size();
        std::cout << "[SDF] " << triCount << " triangles loaded\n";
        if (triCount == 0) { std::cerr << "[SDF] No triangles found\n"; return false; }

        // ── Padded cubic bounds ──
        float cx = (lo.x + hi.x) * 0.5f;
        float cy = (lo.y + hi.y) * 0.5f;
        float cz = (lo.z + hi.z) * 0.5f;
        float maxE = sdfMaxF(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z) * 0.6f;

        SdfVec3 bMin(cx - maxE, cy - maxE, cz - maxE);
        SdfVec3 bMax(cx + maxE, cy + maxE, cz + maxE);
        SdfVec3 ext = bMax.sub(bMin);

        boundsMin[0] = bMin.x; boundsMin[1] = bMin.y; boundsMin[2] = bMin.z;
        boundsExt[0] = ext.x;  boundsExt[1] = ext.y;  boundsExt[2] = ext.z;

        // ── Bake SDF multithreaded ──
        int total = gridRes * gridRes * gridRes;
        std::cout << "[SDF] Baking " << gridRes << "^3 = " << total << " voxels\n";

        std::vector<float> voxels(total);
        float invRes = 1.0f / (float)gridRes;
        SdfVec3 rayDir(0.0f, 1.0f, 0.0f);

        int numThreads = std::thread::hardware_concurrency();
        if (numThreads < 1) numThreads = 4;
        std::cout << "[SDF] Using " << numThreads << " threads\n";

        std::atomic<int> slicesDone(0);
        auto startTime = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int t = 0; t < numThreads; t++) {
            threads.push_back(std::thread([&, t]() {
                for (int iz = t; iz < gridRes; iz += numThreads) {
                    for (int iy = 0; iy < gridRes; iy++) {
                        for (int ix = 0; ix < gridRes; ix++) {

                            float wx = bMin.x + ext.x * ((float)ix + 0.5f) * invRes;
                            float wy = bMin.y + ext.y * ((float)iy + 0.5f) * invRes;
                            float wz = bMin.z + ext.z * ((float)iz + 0.5f) * invRes;
                            SdfVec3 voxelPos(wx, wy, wz);

                            float closestDist = 1e20f;
                            int rayHits = 0;

                            for (uint32_t ti = 0; ti < triCount; ti++) {
                                float dd = sdfPtTriDist(voxelPos, triA[ti], triB[ti], triC[ti]);
                                if (dd < closestDist) closestDist = dd;
                                if (sdfRayHit(voxelPos, rayDir, triA[ti], triB[ti], triC[ti]))
                                    rayHits++;
                            }

                            float sn = (rayHits % 2 == 1) ? -1.0f : 1.0f;
                            voxels[ix + iy * gridRes + iz * gridRes * gridRes] = sn * closestDist;
                        }
                    }

                    int done = ++slicesDone;
                    auto now = std::chrono::high_resolution_clock::now();
                    float secs = std::chrono::duration<float>(now - startTime).count();
                    float pct = (float)done / (float)gridRes * 100.0f;
                    float eta = (done > 0) ? (secs / done) * (gridRes - done) : 0.0f;
                    std::printf("[SDF] %d/%d (%.0f%%) %.0fs elapsed, ~%.0fs remaining\n",
                        done, gridRes, pct, secs, eta);
                }
                }));
        }

        for (int i = 0; i < (int)threads.size(); i++) {
            threads[i].join();
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        float totalSecs = std::chrono::duration<float>(endTime - startTime).count();
        std::printf("[SDF] Bake done in %.1f seconds\n", totalSecs);

        // ── Upload 3D texture ──
        if (texID != 0) glDeleteTextures(1, &texID);
        glGenTextures(1, &texID);
        glBindTexture(GL_TEXTURE_3D, texID);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F,
            gridRes, gridRes, gridRes, 0,
            GL_RED, GL_FLOAT, voxels.data());
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        std::printf("[SDF] Texture uploaded. ID: %d\n", texID);
        return true;
    }

    void bind(GLuint prog, int unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_3D, texID);
        glUniform1i(glGetUniformLocation(prog, "uSdf"), unit);
        glUniform3f(glGetUniformLocation(prog, "uSdfMin"),
            boundsMin[0], boundsMin[1], boundsMin[2]);
        glUniform3f(glGetUniformLocation(prog, "uSdfExt"),
            boundsExt[0], boundsExt[1], boundsExt[2]);
    }

    void destroy() {
        if (texID != 0) {
            glDeleteTextures(1, &texID);
            texID = 0;
        }
    }
};