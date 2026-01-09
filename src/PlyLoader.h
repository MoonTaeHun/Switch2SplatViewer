#ifndef PLYLOADER_H
#define PLYLOADER_H

#include <QString>
#include <vector>
#include "GaussianData.h"

class PlyLoader
{
public:
    PlyLoader();

    // 파일을 읽어서 가공된 데이터(RenderSplat 목록)를 반환
    bool loadPly(const QString &filePath, std::vector<RenderSplat> &outSplats);

private:
    // 0~255 범위로 변환 등을 수행하는 헬퍼 함수
    float sigmoid(float x);
};

#endif // PLYLOADER_H
