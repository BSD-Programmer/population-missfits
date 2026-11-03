#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace misfits {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const {
        float inv = (std::fabs(s) > 1e-6f) ? (1.0f / s) : 0.0f;
        return {x * inv, y * inv, z * inv};
    }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }
    Vec3 normalized() const {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f, 0.0f};
        return {x / len, y / len, z / len};
    }
};

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
    return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
}

struct Color4 {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    static Quat identity() { return {0.0f, 0.0f, 0.0f, 1.0f}; }

    static Quat fromAxisAngle(const Vec3& axis, float radians) {
        Vec3 n = axis.normalized();
        float half = radians * 0.5f;
        float s = std::sin(half);
        return {n.x * s, n.y * s, n.z * s, std::cos(half)};
    }

    static Quat fromEulerYXZ(float yawRad, float pitchRad, float rollRad = 0.0f) {
        Quat qy = fromAxisAngle({0.0f, 1.0f, 0.0f}, yawRad);
        Quat qx = fromAxisAngle({1.0f, 0.0f, 0.0f}, pitchRad);
        Quat qz = fromAxisAngle({0.0f, 0.0f, 1.0f}, rollRad);
        return qy * (qx * qz);
    }

    Quat operator*(const Quat& o) const {
        return {
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w,
            w * o.w - x * o.x - y * o.y - z * o.z
        };
    }

    Vec3 rotate(const Vec3& v) const {
        Vec3 qv{x, y, z};
        Vec3 t = cross(qv, v) * 2.0f;
        return v + t * w + cross(qv, t);
    }

    Vec3 forward() const { return rotate({0.0f, 0.0f, -1.0f}); }
    Vec3 right() const { return rotate({1.0f, 0.0f, 0.0f}); }
    Vec3 up() const { return rotate({0.0f, 1.0f, 0.0f}); }
};

struct AABB {
    Vec3 min{0.0f, 0.0f, 0.0f};
    Vec3 max{0.0f, 0.0f, 0.0f};

    bool contains(const Vec3& p, float expand = 0.0f) const {
        return p.x >= (min.x - expand) && p.x <= (max.x + expand) &&
               p.y >= (min.y - expand) && p.y <= (max.y + expand) &&
               p.z >= (min.z - expand) && p.z <= (max.z + expand);
    }

    bool intersectsRay(const Vec3& origin, const Vec3& dir, float maxDist, float& outT, Vec3& outNormal) const {
        float tmin = 0.0f;
        float tmax = maxDist;
        Vec3 hitN{0.0f, 1.0f, 0.0f};

        const float orig[3] = {origin.x, origin.y, origin.z};
        const float d[3] = {dir.x, dir.y, dir.z};
        const float bmin[3] = {min.x, min.y, min.z};
        const float bmax[3] = {max.x, max.y, max.z};

        for (int i = 0; i < 3; ++i) {
            if (std::fabs(d[i]) < 1e-6f) {
                if (orig[i] < bmin[i] || orig[i] > bmax[i]) return false;
            } else {
                float invD = 1.0f / d[i];
                float t1 = (bmin[i] - orig[i]) * invD;
                float t2 = (bmax[i] - orig[i]) * invD;
                float sign = -1.0f;
                if (t1 > t2) {
                    std::swap(t1, t2);
                    sign = 1.0f;
                }
                if (t1 > tmin) {
                    tmin = t1;
                    hitN = {
                        (i == 0) ? sign : 0.0f,
                        (i == 1) ? sign : 0.0f,
                        (i == 2) ? sign : 0.0f
                    };
                }
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return false;
            }
        }
        outT = tmin;
        outNormal = hitN;
        return tmin <= maxDist && tmax >= 0.0f;
    }
};

struct Mat4 {
    float m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    static Mat4 identity() { return Mat4{}; }

    static Mat4 translation(const Vec3& t) {
        Mat4 r;
        r.m[12] = t.x;
        r.m[13] = t.y;
        r.m[14] = t.z;
        return r;
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 r;
        r.m[0] = s.x;
        r.m[5] = s.y;
        r.m[10] = s.z;
        return r;
    }

    static Mat4 fromQuat(const Quat& q) {
        Mat4 r;
        float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
        float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
        float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

        r.m[0]  = 1.0f - 2.0f * (yy + zz);
        r.m[1]  = 2.0f * (xy + wz);
        r.m[2]  = 2.0f * (xz - wy);
        r.m[3]  = 0.0f;

        r.m[4]  = 2.0f * (xy - wz);
        r.m[5]  = 1.0f - 2.0f * (xx + zz);
        r.m[6]  = 2.0f * (yz + wx);
        r.m[7]  = 0.0f;

        r.m[8]  = 2.0f * (xz + wy);
        r.m[9]  = 2.0f * (yz - wx);
        r.m[10] = 1.0f - 2.0f * (xx + yy);
        r.m[11] = 0.0f;

        r.m[12] = 0.0f;
        r.m[13] = 0.0f;
        r.m[14] = 0.0f;
        r.m[15] = 1.0f;
        return r;
    }

    static Mat4 trs(const Vec3& pos, const Quat& rot, const Vec3& scl) {
        Mat4 r = fromQuat(rot);
        r.m[0] *= scl.x; r.m[1] *= scl.x; r.m[2] *= scl.x;
        r.m[4] *= scl.y; r.m[5] *= scl.y; r.m[6] *= scl.y;
        r.m[8] *= scl.z; r.m[9] *= scl.z; r.m[10] *= scl.z;
        r.m[12] = pos.x;
        r.m[13] = pos.y;
        r.m[14] = pos.z;
        return r;
    }

    static Mat4 perspectiveAsymmetric(float angleLeft, float angleRight, float angleUp, float angleDown, float nearZ, float farZ) {
        const float tanLeft = std::tan(angleLeft);
        const float tanRight = std::tan(angleRight);
        const float tanDown = std::tan(angleDown);
        const float tanUp = std::tan(angleUp);

        const float tanWidth = tanRight - tanLeft;
        const float tanHeight = tanUp - tanDown;

        Mat4 res{};
        res.m[0] = 2.0f / tanWidth;
        res.m[5] = 2.0f / tanHeight;
        res.m[8] = (tanRight + tanLeft) / tanWidth;
        res.m[9] = (tanUp + tanDown) / tanHeight;
        res.m[10] = -(farZ + nearZ) / (farZ - nearZ);
        res.m[11] = -1.0f;
        res.m[14] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
        res.m[15] = 0.0f;
        return res;
    }

    static Mat4 viewFromPose(const Vec3& pos, const Quat& rot) {
        Quat invRot{-rot.x, -rot.y, -rot.z, rot.w};
        Mat4 r = fromQuat(invRot);
        Vec3 invPos = invRot.rotate({-pos.x, -pos.y, -pos.z});
        r.m[12] = invPos.x;
        r.m[13] = invPos.y;
        r.m[14] = invPos.z;
        return r;
    }

    Mat4 operator*(const Mat4& b) const {
        Mat4 out{};
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                out.m[col * 4 + row] =
                    m[0 * 4 + row] * b.m[col * 4 + 0] +
                    m[1 * 4 + row] * b.m[col * 4 + 1] +
                    m[2 * 4 + row] * b.m[col * 4 + 2] +
                    m[3 * 4 + row] * b.m[col * 4 + 3];
            }
        }
        return out;
    }
};

} // namespace misfits
