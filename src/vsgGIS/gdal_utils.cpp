/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgGIS/gdal_utils.h>

#include <cstring>

using namespace vsgGIS;

bool vsgGIS::compatibleDatasetProjections(const GDALDataset& lhs, const GDALDataset& rhs)
{
    if (&lhs == &rhs) return true;

    const auto lhs_projectionRef = const_cast<GDALDataset&>(lhs).GetProjectionRef();
    const auto rhs_projectionRef = const_cast<GDALDataset&>(lhs).GetProjectionRef();

    // if pointers are the same then they are compatible
    if (lhs_projectionRef == rhs_projectionRef) return true;

    // if one of the pointers is NULL then they are incompatible
    if (!lhs_projectionRef || !rhs_projectionRef) return false;

    // check in the OGRSpatialReference are the same
    return (std::strcmp(lhs_projectionRef, rhs_projectionRef) == 0);
}

bool vsgGIS::compatibleDatasetProjectionsTransformAndSizes(const GDALDataset& lhs, const GDALDataset& rhs)
{
    if (!compatibleDatasetProjections(lhs, rhs)) return false;

    auto& non_const_lhs = const_cast<GDALDataset&>(lhs);
    auto& non_const_rhs = const_cast<GDALDataset&>(rhs);

    if (non_const_lhs.GetRasterXSize() != non_const_rhs.GetRasterXSize() || non_const_lhs.GetRasterYSize() != non_const_rhs.GetRasterYSize())
    {
        return false;
    }

    double lhs_GeoTransform[6];
    double rhs_GeoTransform[6];

    int numberWithValidTransforms = 0;
    if (non_const_lhs.GetGeoTransform(lhs_GeoTransform) == CE_None) ++numberWithValidTransforms;
    if (non_const_rhs.GetGeoTransform(rhs_GeoTransform) == CE_None) ++numberWithValidTransforms;

    // if neither have transform mark as compatible
    if (numberWithValidTransforms == 0) return true;

    // only one has a transform so must be incompatible
    if (numberWithValidTransforms == 1) return false;

    for (int i = 0; i < 6; ++i)
    {
        if (lhs_GeoTransform[i] != rhs_GeoTransform[i])
        {
            return false;
        }
    }
    return true;
}

template<typename T>
T default_value(double scale)
{
    if (std::numeric_limits<T>::is_integer)
    {
        if (scale<0.0) return std::numeric_limits<T>::min();
        else if (scale>0.0) return std::numeric_limits<T>::max();
        return static_cast<T>(0);
    }
    else
    {
        return static_cast<T>(scale);
    }
}

template<typename T>
vsg::t_vec2<T> default_vec2(const vsg::dvec4& value)
{
    return vsg::t_vec2<T>(default_value<T>(value[0]), default_value<T>(value[1]));
}

template<typename T>
vsg::t_vec3<T> default_vec3(const vsg::dvec4& value)
{
    return vsg::t_vec3<T>(default_value<T>(value[0]), default_value<T>(value[1]), default_value<T>(value[2]));
}

template<typename T>
vsg::t_vec4<T> default_vec4(const vsg::dvec4& value)
{
    return vsg::t_vec4<T>(default_value<T>(value[0]), default_value<T>(value[1]), default_value<T>(value[2]), default_value<T>(value[4]));
}


vsg::ref_ptr<vsg::Data> vsgGIS::createImage2D(int width, int height, int numComponents, GDALDataType dataType, vsg::dvec4 def)
{
    using TypeCompontents = std::pair<GDALDataType, int>;
    using CreateFunction = std::function<vsg::ref_ptr<vsg::Data>(uint32_t w, uint32_t h, vsg::dvec4 def)>;

    std::map<TypeCompontents, CreateFunction> createMap;
    createMap[TypeCompontents(GDT_Byte, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ubyteArray2D::create(w, h, default_value<uint8_t>(d[0]), vsg::Data::Layout{VK_FORMAT_R8_UNORM}) ; };
    createMap[TypeCompontents(GDT_UInt16, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ushortArray2D::create(w, h, default_value<uint16_t>(d[0]), vsg::Data::Layout{VK_FORMAT_R16_UNORM}); };
    createMap[TypeCompontents(GDT_Int16, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::shortArray2D::create(w, h, default_value<int16_t>(d[0]), vsg::Data::Layout{VK_FORMAT_R16_SNORM}); };
    createMap[TypeCompontents(GDT_UInt32, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::uintArray2D::create(w, h, default_value<uint32_t>(d[0]), vsg::Data::Layout{VK_FORMAT_R32_UINT}); };
    createMap[TypeCompontents(GDT_Int32, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::intArray2D::create(w, h, default_value<int32_t>(d[0]), vsg::Data::Layout{VK_FORMAT_R32_SINT}); };
    createMap[TypeCompontents(GDT_Float32, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::floatArray2D::create(w, h, default_value<float>(d[0]), vsg::Data::Layout{VK_FORMAT_R32_SFLOAT}); };
    createMap[TypeCompontents(GDT_Float64, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::doubleArray2D::create(w, h, default_value<double>(d[0]), vsg::Data::Layout{VK_FORMAT_R64_SFLOAT}); };

    createMap[TypeCompontents(GDT_Byte, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ubvec2Array2D::create(w, h, default_vec2<uint8_t>(d), vsg::Data::Layout{VK_FORMAT_R8G8_UNORM}); };
    createMap[TypeCompontents(GDT_UInt16, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::usvec2Array2D::create(w, h, default_vec2<uint16_t>(d), vsg::Data::Layout{VK_FORMAT_R16G16_UNORM}); };
    createMap[TypeCompontents(GDT_Int16, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::svec2Array2D::create(w, h, default_vec2<int16_t>(d), vsg::Data::Layout{VK_FORMAT_R16G16_SNORM}); };
    createMap[TypeCompontents(GDT_UInt32, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::uivec2Array2D::create(w, h, default_vec2<uint32_t>(d), vsg::Data::Layout{VK_FORMAT_R32G32_UINT}); };
    createMap[TypeCompontents(GDT_Int32, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ivec2Array2D::create(w, h, default_vec2<int32_t>(d), vsg::Data::Layout{VK_FORMAT_R32G32_SINT}); };
    createMap[TypeCompontents(GDT_Float32, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::vec2Array2D::create(w, h, default_vec2<float>(d), vsg::Data::Layout{VK_FORMAT_R32G32_SFLOAT}); };
    createMap[TypeCompontents(GDT_Float64, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::dvec2Array2D::create(w, h, default_vec2<double>(d), vsg::Data::Layout{VK_FORMAT_R64G64_SFLOAT}); };

    createMap[TypeCompontents(GDT_Byte, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ubvec3Array2D::create(w, h, default_vec3<uint8_t>(d), vsg::Data::Layout{VK_FORMAT_R8G8B8_UNORM}); };
    createMap[TypeCompontents(GDT_UInt16, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::usvec3Array2D::create(w, h, default_vec3<uint16_t>(d), vsg::Data::Layout{VK_FORMAT_R16G16B16_UNORM}); };
    createMap[TypeCompontents(GDT_Int16, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::svec3Array2D::create(w, h, default_vec3<int16_t>(d), vsg::Data::Layout{VK_FORMAT_R16G16B16_SNORM}); };
    createMap[TypeCompontents(GDT_UInt32, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::uivec3Array2D::create(w, h, default_vec3<uint32_t>(d), vsg::Data::Layout{VK_FORMAT_R32G32B32_UINT}); };
    createMap[TypeCompontents(GDT_Int32, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ivec3Array2D::create(w, h, default_vec3<int32_t>(d), vsg::Data::Layout{VK_FORMAT_R32G32B32_SINT}); };
    createMap[TypeCompontents(GDT_Float32, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::vec3Array2D::create(w, h, default_vec3<float>(d), vsg::Data::Layout{VK_FORMAT_R32G32B32_SFLOAT}); };
    createMap[TypeCompontents(GDT_Float64, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::dvec3Array2D::create(w, h, default_vec3<double>(d), vsg::Data::Layout{VK_FORMAT_R64G64B64_SFLOAT}); };

    createMap[TypeCompontents(GDT_Byte, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ubvec4Array2D::create(w, h, default_vec4<uint8_t>(d), vsg::Data::Layout{VK_FORMAT_R8G8B8A8_UNORM}); };
    createMap[TypeCompontents(GDT_UInt16, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::usvec4Array2D::create(w, h, default_vec4<uint16_t>(d), vsg::Data::Layout{VK_FORMAT_R16G16B16A16_UNORM}); };
    createMap[TypeCompontents(GDT_Int16, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::svec4Array2D::create(w, h, default_vec4<int16_t>(d), vsg::Data::Layout{VK_FORMAT_R16G16B16A16_SNORM}); };
    createMap[TypeCompontents(GDT_UInt32, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::uivec4Array2D::create(w, h, default_vec4<uint32_t>(d), vsg::Data::Layout{VK_FORMAT_R32G32B32A32_UINT}); };
    createMap[TypeCompontents(GDT_Int32, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ivec4Array2D::create(w, h, default_vec4<int32_t>(d), vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SINT}); };
    createMap[TypeCompontents(GDT_Float32, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::vec4Array2D::create(w, h, default_vec4<float>(d), vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT}); };
    createMap[TypeCompontents(GDT_Float64, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::dvec4Array2D::create(w, h, default_vec4<double>(d), vsg::Data::Layout{VK_FORMAT_R64G64B64A64_SFLOAT}); };

    auto itr = createMap.find(TypeCompontents(dataType, numComponents));
    if (itr == createMap.end())
    {
        return {};
    }

    return (itr->second)(width, height, def);
}
