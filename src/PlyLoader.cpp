#include "PlyLoader.h"
#include <QFile>
#include <QTextStream>
#include <QDataStream>
#include <QDebug>
#include <cmath>

PlyLoader::PlyLoader() {}

float PlyLoader::sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

bool PlyLoader::loadPly(const QString &filePath, std::vector<RenderSplat> &outSplats)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open file:" << filePath;
        return false;
    }

    // --- 1. Header Parsing ---
    int vertexCount = 0;
    bool isBinary = false;

    while (!file.atEnd()) {
        QByteArray line = file.readLine().trimmed();
        if (line == "end_header") break;

        if (line.startsWith("format binary_little_endian")) {
            isBinary = true;
        }
        else if (line.startsWith("element vertex")) {
            // "element vertex 123456" 형태에서 숫자만 추출
            QList<QByteArray> parts = line.split(' ');
            if (parts.size() >= 3) {
                vertexCount = parts[2].toInt();
            }
        }
    }

    if (vertexCount <= 0 || !isBinary) {
        qCritical() << "Invalid PLY format or empty file.";
        return false;
    }

    qDebug() << "Loading" << vertexCount << "splats...";

    // --- 2. Binary Body Reading ---
    // 전체 데이터를 담을 버퍼 준비
    // 주의: 표준 3DGS PLY 포맷 순서를 가정합니다.
    // (x,y,z, nx,ny,nz, f_dc_0,1,2, f_rest(45개), opacity, scale_0,1,2, rot_0,1,2,3)
    // 총 62개의 float = 248 bytes per splat

    // 파일 포인터는 현재 'end_header' 다음 줄(바이너리 시작점)에 있음
    QByteArray data = file.readAll();

    // 데이터 크기 검증 (간단히)
    // const int splatSize = 62 * sizeof(float);
    // 실제로는 property 순서에 따라 다를 수 있으나, 표준 학습 모델 결과물 기준

    const float* rawData = reinterpret_cast<const float*>(data.constData());

    outSplats.clear();
    outSplats.reserve(vertexCount);

    // 표준 구조체 스트라이드 (float 개수)
    // x,y,z(3) + n(3) + f_dc(3) + f_rest(45) + op(1) + scale(3) + rot(4) = 62
    const int STRIDE = 62;

    for (int i = 0; i < vertexCount; ++i) {
        int base = i * STRIDE;

        // 파일 끝 체크
        if (base + STRIDE > data.size() / sizeof(float)) break;

        RenderSplat s;

        // 1. Position
        s.x = rawData[base + 0];
        s.y = rawData[base + 1];
        s.z = rawData[base + 2];

        // 2. Color (f_dc)
        // SH 0th order는 RGB로 변환 시 0.28209... 상수가 붙음 + 0.5 offset
        const float SH_C0 = 0.28209479177387814f;
        s.r = (0.5f + SH_C0 * rawData[base + 6]);
        s.g = (0.5f + SH_C0 * rawData[base + 7]);
        s.b = (0.5f + SH_C0 * rawData[base + 8]);

        // 클램핑 (0~1)
        if (s.r < 0) s.r = 0; if (s.r > 1) s.r = 1;
        if (s.g < 0) s.g = 0; if (s.g > 1) s.g = 1;
        if (s.b < 0) s.b = 0; if (s.b > 1) s.b = 1;

        // 3. Opacity (Sigmoid 적용 필요)
        s.opacity = sigmoid(rawData[base + 54]);

        // 4. Scale (Exp 적용 필요)
        s.scale[0] = std::exp(rawData[base + 55]);
        s.scale[1] = std::exp(rawData[base + 56]);
        s.scale[2] = std::exp(rawData[base + 57]);

        // 5. Rotation
        s.rot[0] = rawData[base + 58];
        s.rot[1] = rawData[base + 59];
        s.rot[2] = rawData[base + 60];
        s.rot[3] = rawData[base + 61];

        outSplats.push_back(s);
    }

    qDebug() << "Successfully loaded" << outSplats.size() << "splats.";
    file.close();
    return true;
}
