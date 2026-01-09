#ifndef GAUSSIANDATA_H
#define GAUSSIANDATA_H

#include <vector>
#include <QVector3D>

// 점 하나(Splat)가 가지는 속성
struct Splat {
    float x, y, z;          // 위치
    float nx, ny, nz;       // 법선 (보통 무시하지만 파일엔 있음)
    float f_dc[3];          // 색상 기초 정보 (0th order SH) - RGB라고 생각하면 됨
    float f_rest[45];       // 추가 색상 정보 (이번엔 사용 안 함, 메모리 정렬용)
    float opacity;          // 투명도 (Sigmoid 통과 전 값)
    float scale[3];         // 크기 (Log 스케일)
    float rot[4];           // 회전 (Quaternion: r, x, y, z)
};

// 렌더링에 필요한 핵심 데이터만 추린 구조체 (최적화용)
struct RenderSplat {
    float x, y, z;      // 위치
    float r, g, b;      // 색상
    float opacity;      // 투명도
    float scale[3];
    float rot[4];
};

#endif // GAUSSIANDATA_H
