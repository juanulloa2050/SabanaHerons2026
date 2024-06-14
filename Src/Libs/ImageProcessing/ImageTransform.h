/**
 * @author <a href="mailto:jesse@tzi.de">Jesse Richter-Klug</a>
 */
#pragma once

#include "ImageProcessing/AVX.h"
#include "ImageProcessing/Image.h"
#include "Math/Eigen.h"
#include "Platform/BHAssert.h"

namespace ImageTransform
{
  template<typename T>
  ALWAYSINLINE static __m128 setR2float(T, T, T, T);
  template<> ALWAYSINLINE __m128 setR2float(unsigned char v0, unsigned char v1, unsigned char v2, unsigned char v3)
  {
    return _mm_cvtepi32_ps(_mm_setr_epi32(v0, v1, v2, v3));
  }
  template<> ALWAYSINLINE __m128 setR2float(float v0, float v1, float v2, float v3)
  {
    return _mm_setr_ps(v0, v1, v2, v3);
  }

  template<typename T>
ALWAYSINLINE static __m128 getPixel(const Image<T>& src, __m128 x_x_x_x_Dash, __m128 y_y_y_y_Dash, float defaultValue = 0.f)
{
  // Convert floating point coordinates to integer
  const __m128i xCoordsInt = _mm_cvtps_epi32(x_x_x_x_Dash);
  const __m128i yCoordsInt = _mm_cvtps_epi32(y_y_y_y_Dash);

  // Calculate weights for bilinear interpolation
  const __m128 xWeightsRight = _mm_sub_ps(x_x_x_x_Dash, _mm_cvtepi32_ps(xCoordsInt));
  const __m128 yWeightsLower = _mm_sub_ps(y_y_y_y_Dash, _mm_cvtepi32_ps(yCoordsInt));
  const __m128 xWeightsLeft = _mm_sub_ps(_mm_set1_ps(1.f), xWeightsRight);
  const __m128 yWeightsUpper = _mm_sub_ps(_mm_set1_ps(1.f), yWeightsLower);

  // Check if coordinates are within image bounds
  const __m128i maskXInRange = _mm_and_si128(_mm_cmpgt_epi32(xCoordsInt, _mm_set1_epi32(-1)),
                                             _mm_cmpgt_epi32(_mm_set1_epi32(src.width - 1), xCoordsInt));
  const __m128i maskYInRange = _mm_and_si128(_mm_cmpgt_epi32(yCoordsInt, _mm_set1_epi32(-1)),
                                             _mm_cmpgt_epi32(_mm_set1_epi32(src.height - 1), yCoordsInt));
  const __m128i maskInRange = _mm_and_si128(maskXInRange, maskYInRange);

  // Initialize arrays to store valid coordinates
  int32_t xCoordinates[4], yCoordinates[4];
  _mm_store_si128(reinterpret_cast<__m128i*>(xCoordinates), _mm_and_si128(maskInRange, xCoordsInt));
  _mm_store_si128(reinterpret_cast<__m128i*>(yCoordinates), _mm_and_si128(maskInRange, yCoordsInt));

  // Prepare for bilinear interpolation
  __m128 result = _mm_setzero_ps();
  for (int i = 0; i < 4; i += 2) {
    // Fetch pixel values for interpolation
    __m128 upperValues = setR2float(
      src[yCoordinates[i]][xCoordinates[i]], src[yCoordinates[i]][xCoordinates[i] + 1],
      src[yCoordinates[i + 1]][xCoordinates[i + 1]], src[yCoordinates[i + 1]][xCoordinates[i + 1] + 1]);
    __m128 lowerValues = setR2float(
      src[yCoordinates[i] + 1][xCoordinates[i]], src[yCoordinates[i] + 1][xCoordinates[i] + 1],
      src[yCoordinates[i + 1] + 1][xCoordinates[i + 1]], src[yCoordinates[i + 1] + 1][xCoordinates[i + 1] + 1]);

    // Calculate weights for current pair
    __m128 xWeights = i == 0 ? _mm_unpacklo_ps(xWeightsLeft, xWeightsRight) : _mm_unpackhi_ps(xWeightsLeft, xWeightsRight);
    __m128 yWeightsUpper = i == 0 ? _mm_unpacklo_ps(yWeightsUpper, yWeightsUpper) : _mm_unpackhi_ps(yWeightsUpper, yWeightsUpper);
    __m128 yWeightsLower = i == 0 ? _mm_unpacklo_ps(yWeightsLower, yWeightsLower) : _mm_unpackhi_ps(yWeightsLower, yWeightsLower);

    // Perform bilinear interpolation
    result = _mm_add_ps(result, _mm_mul_ps(xWeights, _mm_add_ps(_mm_mul_ps(yWeightsUpper, upperValues), _mm_mul_ps(yWeightsLower, lowerValues))));
  }

  // Clamp result to maximum value and apply mask for in-range coordinates
  result = _mm_min_ps(_mm_and_ps(_mm_castsi128_ps(maskInRange), result), _mm_set1_ps(255.f));

  // Apply default value for out-of-range coordinates
  if (defaultValue != 0.f) {
    const __m128 defaultValues = _mm_set1_ps(defaultValue);
    result = _mm_or_ps(_mm_andnot_ps(_mm_castsi128_ps(maskInRange), defaultValues), result);
  }

  return result;
}

  // writes the result of the affine transformation into dest, which has to be a buffer with a sufficient large size
  void transform(const Image<PixelTypes::GrayscaledPixel>& src, float* destP, unsigned int dest_width, unsigned int dest_height, const Matrix3f& inverseTransformation, const Vector2f& relativTransformationCenter = Vector2f(0.5f, 0.5f), const float defaultValue = 0.f)
  {
    const __m128 a0 = _mm_set1_ps(inverseTransformation(0, 0));
    const __m128 a1 = _mm_set1_ps(inverseTransformation(0, 1));
    const __m128 a2 = _mm_set1_ps(inverseTransformation(0, 2));
    const __m128 a3 = _mm_set1_ps(inverseTransformation(1, 0));
    const __m128 a4 = _mm_set1_ps(inverseTransformation(1, 1));
    const __m128 a5 = _mm_set1_ps(inverseTransformation(1, 2));

    const __m128 _0123 = _mm_setr_ps(0, 1.f, 2.f, 3.f);

    const Vector2f transformationCenter = Vector2f(dest_width, dest_height).cwiseProduct(relativTransformationCenter);
    //const Vector2i transformationCenter_i = transformationCenter.cast<int>();
    const float startX = -transformationCenter.x();
    //const float endX = dest.width - transformationCenter.x();
    const float startY = -transformationCenter.y();
    //const float cendY = dest.height - transformationCenter.y();

    //set the 4 times a things
    const __m128 a0x4 = _mm_mul_ps(a0, _mm_set1_ps(4.f));
    const __m128 a3x4 = _mm_mul_ps(a3, _mm_set1_ps(4.f));
    //calculate the 0,0 transformation; which allows us to only accumulate afterwards
    __m128 preCalculated_x_x_x_x_Dash = _mm_add_ps(_mm_set1_ps(src.width * relativTransformationCenter.x()), _mm_add_ps(_mm_add_ps(a2, _mm_mul_ps(_mm_set1_ps(startY), a1)), _mm_mul_ps(_mm_add_ps(_mm_set1_ps(startX), _0123), a0)));
    __m128 preCalculated_y_y_y_y_Dash = _mm_add_ps(_mm_set1_ps(src.height * relativTransformationCenter.y()), _mm_add_ps(_mm_add_ps(a5, _mm_mul_ps(_mm_set1_ps(startY), a4)), _mm_mul_ps(_mm_add_ps(_mm_set1_ps(startX), _0123), a3)));

    ASSERT(dest_width % 4 == 0);
    for(unsigned y = 0; y < dest_height; ++y)
    {
      //calculate "const" y part of transform
      preCalculated_x_x_x_x_Dash = _mm_add_ps(preCalculated_x_x_x_x_Dash, a1);
      preCalculated_y_y_y_y_Dash = _mm_add_ps(preCalculated_y_y_y_y_Dash, a4);
      // copy so we dont change the precalculation of next line while adding the x part of the transform
      __m128 x_x_x_x_Dash = preCalculated_x_x_x_x_Dash;
      __m128 y_y_y_y_Dash = preCalculated_y_y_y_y_Dash;

      for(unsigned x = 0; x < dest_width; x += 4, destP += 4,

          // finish transformation for the next iteration
          x_x_x_x_Dash = _mm_add_ps(x_x_x_x_Dash, a0x4),
          y_y_y_y_Dash = _mm_add_ps(y_y_y_y_Dash, a3x4))
      {
        _mm_storeu_ps(destP, getPixel(src, x_x_x_x_Dash, y_y_y_y_Dash, defaultValue));
      }
    }
  }

  // just affine right now
  void transform(const Image<unsigned char>& src, Image<float>& dest, const Matrix3f& inverseTransformation, const Vector2f& relativTransformationCenter = Vector2f(0.5f, 0.5f))
  {
    const __m128 a0 = _mm_set1_ps(inverseTransformation(0, 0));
    const __m128 a1 = _mm_set1_ps(inverseTransformation(0, 1));
    const __m128 a2 = _mm_set1_ps(inverseTransformation(0, 2));
    const __m128 a3 = _mm_set1_ps(inverseTransformation(1, 0));
    const __m128 a4 = _mm_set1_ps(inverseTransformation(1, 1));
    const __m128 a5 = _mm_set1_ps(inverseTransformation(1, 2));

    const __m128 _0123 = _mm_setr_ps(0, 1.f, 2.f, 3.f);

    const Vector2f transformationCenter = Vector2f(dest.width, dest.height).cwiseProduct(relativTransformationCenter);
    //const Vector2i transformationCenter_i = transformationCenter.cast<int>();
    const float startX = -transformationCenter.x();
    //const float endX = dest.width - transformationCenter.x();
    const float startY = -transformationCenter.y();
    //const float cendY = dest.height - transformationCenter.y();

    float* destP = dest[0];

    //set the 4 times a things
    const __m128 a0x4 = _mm_mul_ps(a0, _mm_set1_ps(4.f));
    const __m128 a3x4 = _mm_mul_ps(a3, _mm_set1_ps(4.f));
    //calculate the 0,0 transformation; which allows us to only accumulate afterwards
    __m128 preCalculated_x_x_x_x_Dash = _mm_add_ps(_mm_set1_ps(src.width * relativTransformationCenter.x()), _mm_add_ps(_mm_add_ps(a2, _mm_mul_ps(_mm_set1_ps(startY), a1)), _mm_mul_ps(_mm_add_ps(_mm_set1_ps(startX), _0123), a0)));
    __m128 preCalculated_y_y_y_y_Dash = _mm_add_ps(_mm_set1_ps(src.height * relativTransformationCenter.y()), _mm_add_ps(_mm_add_ps(a5, _mm_mul_ps(_mm_set1_ps(startY), a4)), _mm_mul_ps(_mm_add_ps(_mm_set1_ps(startX), _0123), a3)));

    ASSERT(dest.width % 4 == 0);
    for(unsigned y = 0; y < dest.height; ++y)
    {
      //calculate "const" y part of transform
      preCalculated_x_x_x_x_Dash = _mm_add_ps(preCalculated_x_x_x_x_Dash, a1);
      preCalculated_y_y_y_y_Dash = _mm_add_ps(preCalculated_y_y_y_y_Dash, a4);
      // copy so we dont change the precalculation of next line while adding the x part of the transform
      __m128 x_x_x_x_Dash = preCalculated_x_x_x_x_Dash;
      __m128 y_y_y_y_Dash = preCalculated_y_y_y_y_Dash;

      for(unsigned x = 0; x < dest.width; x += 4, destP += 4,

          // finish transformation for the next iteration
          x_x_x_x_Dash = _mm_add_ps(x_x_x_x_Dash, a0x4),
          y_y_y_y_Dash = _mm_add_ps(y_y_y_y_Dash, a3x4))
      {
        _mm_storeu_ps(destP, getPixel(src, x_x_x_x_Dash, y_y_y_y_Dash));
      }
    }
  }

  void transform(const Image<float>& src, Image<unsigned char>& dest)
  {
    dest.setResolution(src.width, src.height);

    const float* const srcSSEend = src[0] + (src.width * src.height) / 16u * 16u;
    //const float*const srcEnd = &src[src.height - 1][src.width];

    __m128i* destP = reinterpret_cast<__m128i*>(dest[0]) - 1;

    const float* srcP = src[0] - 4;
    while(srcP < srcSSEend)
    {
      const __m128i scr0 = _mm_cvtps_epi32(_mm_load_ps(srcP += 4));
      const __m128i scr1 = _mm_cvtps_epi32(_mm_load_ps(srcP += 4));
      const __m128i scr2 = _mm_cvtps_epi32(_mm_load_ps(srcP += 4));
      const __m128i scr3 = _mm_cvtps_epi32(_mm_load_ps(srcP += 4));

      _mm_store_si128(++destP, _mm_packus_epi16(_mm_packs_epi32(scr0, scr1), _mm_packs_epi32(scr2, scr3)));
    }
  }

  ////////
  // TODO opimize below

void polarTransform(const Image<float>& src, Image<float>& dest)
{
  const float angleDiff = static_cast<float>(2.f * M_PI / dest.width);
  const float rDiff = static_cast<float>(src.height / 2.f / dest.height);

  std::vector<float> precalcXDiff(dest.width);
  std::vector<float> precalcYDiff(dest.width);

  for(unsigned i = 0; i < dest.width; ++i)
  {
    const float angle = static_cast<float>(i * angleDiff); // Explicit cast
    precalcXDiff[i] = static_cast<float>(std::cos(angle) * rDiff); // Explicit cast
    precalcYDiff[i] = static_cast<float>(std::sin(angle) * rDiff); // Explicit cast
  }

  ASSERT(dest.width % 4 == 0);
  for(unsigned x = 0; x < dest.width; x += 4)
  {
    for(unsigned y = 0; y < dest.height; ++y)
    {
      __m128 x_base = _mm_set1_ps(src.width / 2.f);
      __m128 y_base = _mm_set1_ps(src.height / 2.f + y * rDiff);

      __m128 xDiff = _mm_loadu_ps(&precalcXDiff[x]);
      __m128 yDiff = _mm_loadu_ps(&precalcYDiff[x]);

      __m128 x_pos = _mm_add_ps(x_base, xDiff);
      __m128 y_pos = _mm_add_ps(y_base, yDiff);

      _mm_storeu_ps(&dest[y][x], getPixel(src, x_pos, y_pos));
    }
  }
}

  void logPolarTransform(const Image<float>& src, Image<float>& dest)
  {
    // for normal images first
    const float angelDiff = 2.f * pi / dest.width;
    const float rDiff = std::log2(src.height / 2.f) / dest.height;

    std::vector<float> precalcXDirection;
    std::vector<float> precalcYDirection;

    for(unsigned i = 0; i < dest.width; ++i)
    {
      const float angle = i * angelDiff;
      precalcXDirection.emplace_back(std::cos(angle));
      precalcYDirection.emplace_back(std::sin(angle));
    }

    ASSERT(dest.width % 4 == 0);
    for(unsigned x = 0; x < dest.width; x += 4)
    {
      const __m128 center_x = _mm_set1_ps(src.width / 2.f);
      const __m128 center_y = _mm_set1_ps(src.height / 2.f);

      const __m128 xDir = _mm_loadu_ps(&precalcXDirection[x]);
      const __m128 yDir = _mm_loadu_ps(&precalcYDirection[x]);

      for(unsigned y = 0; y < dest.height; ++y)
      {
        const __m128 x_x_x_x = _mm_add_ps(center_x, _mm_mul_ps(xDir, _mm_set1_ps(std::pow(2.f, y * rDiff))));
        const __m128 y_y_y_y = _mm_add_ps(center_y, _mm_mul_ps(yDir, _mm_set1_ps(std::pow(2.f, y * rDiff))));
        _mm_storeu_ps(&dest[y][x], getPixel(src, x_x_x_x, y_y_y_y));
      }
    }
  }

  //todo make just one method
  void halfImagePolarTransform(const Image<float>& src, Image<float>& dest)
  {
    const float angelDiff = pi / dest.width;
    const float rDiff = std::min(src.height / 2.f, 1.f * src.width) / dest.height;

    std::vector<float> precalcXDiff;
    std::vector<float> precalcYDiff;

    for(int i = -static_cast<int>(dest.width) / 2; i < static_cast<int>(dest.width / 2); ++i)
    {
      const float angle = i * angelDiff;
      precalcXDiff.emplace_back(std::cos(angle) * rDiff);
      precalcYDiff.emplace_back(std::sin(angle) * rDiff);
    }

    ASSERT(dest.width % 4 == 0);
    for(unsigned x = 0; x < dest.width; x += 4)
    {
      __m128 x_x_x_x = _mm_setzero_ps();
      __m128 y_y_y_y = _mm_set1_ps(src.height / 2.f);

      const __m128 xDiff = _mm_loadu_ps(&precalcXDiff[x]);
      const __m128 yDiff = _mm_loadu_ps(&precalcYDiff[x]);

      for(unsigned y = 0; y < dest.height; ++y)
      {
        x_x_x_x = _mm_add_ps(x_x_x_x, xDiff);
        y_y_y_y = _mm_add_ps(y_y_y_y, yDiff);
        _mm_storeu_ps(&dest[y][x], getPixel(src, x_x_x_x, y_y_y_y));
      }
    }
  }
}
