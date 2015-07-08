#pragma once

// Transformation types handled, kind of redundant with itk transforms but much easier to use after
// TRANSLATION -> itk::TranslationTransform
// RIGID -> itk::Rigid3DTransform
// AFFINE -> itk::AffineTransform

#include <itkMatrixOffsetTransformBase.h>
#include <itkImage.h>
#include <itkPoint.h>

namespace anima
{

template <unsigned int NDimensions = 3>
class BaseTransformAgregator
{
public:
    typedef double ScalarType;
    typedef double InternalScalarType;
    typedef itk::Transform<InternalScalarType,NDimensions,NDimensions> BaseInputTransformType;
    typedef typename BaseInputTransformType::Pointer BaseInputTransformPointer;
    typedef itk::Transform<ScalarType,NDimensions,NDimensions> BaseOutputTransformType;
    typedef itk::Point <InternalScalarType,NDimensions> PointType;
    typedef itk::ImageRegion <NDimensions> RegionType;

    enum TRANSFORM_TYPE {
        TRANSLATION,
        RIGID,
        AFFINE,
        SVF
    };

    BaseTransformAgregator();
    virtual ~BaseTransformAgregator();

    void SetInputTransforms(std::vector <BaseInputTransformPointer> &inputTransforms);

    std::vector <BaseInputTransformPointer> &GetInputTransforms() {return m_InputTransforms;}
    BaseInputTransformType *GetInputTransform(unsigned int i) {return m_InputTransforms[i].GetPointer();}

    void SetInputOrigins(const std::vector <PointType> &inputOrigins)
    {
        m_InputOrigins = inputOrigins;
        m_UpToDate = false;
    }

    template <class TInputImageType> void SetInputRegions(const TInputImageType *refImage, const std::vector <RegionType> &inputRegions)
    {
        m_InputOrigins.clear();
        m_InputOrigins.resize(inputRegions.size());

        itk::ContinuousIndex <InternalScalarType, NDimensions> tmpCenter;

        for (unsigned int i = 0;i < inputRegions.size();++i)
        {
            for (unsigned int j = 0;j < NDimensions;++j)
                tmpCenter[j] = inputRegions[i].GetIndex()[j] + (inputRegions[i].GetSize()[j] - 1.0) / 2.0;

            refImage->TransformContinuousIndexToPhysicalPoint(tmpCenter,m_InputOrigins[i]);
        }

        m_InputRegions = inputRegions;

        m_UpToDate = false;
    }

    std::vector <RegionType> & GetInputRegions() {return m_InputRegions;}
    std::vector <PointType> & GetInputOrigins() {return m_InputOrigins;}
    PointType & GetInputOrigin(unsigned int i) {return m_InputOrigins[i];}

    void SetInputWeights(const std::vector <InternalScalarType> &weights)
    {
        m_Weights = weights;
        m_UpToDate = false;
    }

    std::vector <InternalScalarType> & GetInputWeights() {return m_Weights;}
    InternalScalarType GetInputWeight(unsigned int i) {return m_Weights[i];}
    void SetInputWeight(unsigned int i, double w)
    {
        if (m_Weights.size() > i)
            m_Weights[i] = w;
    }

    void SetInputTransformType(TRANSFORM_TYPE name);
    void SetOutputTransformType(TRANSFORM_TYPE name);

    void SetUpToDate(bool value) {m_UpToDate = value;}

    TRANSFORM_TYPE GetInputTransformType() {return m_InputTransformType;}
    TRANSFORM_TYPE GetOutputTransformType() {return m_OutputTransformType;}

    virtual bool Update() = 0;

    BaseOutputTransformType *GetOutput();

protected:
    void SetOutput(BaseOutputTransformType *output);

private:
    std::vector <BaseInputTransformPointer> m_InputTransforms;
    std::vector <PointType> m_InputOrigins;
    std::vector <InternalScalarType> m_Weights;

    bool m_UpToDate;

    typename BaseOutputTransformType::Pointer m_Output;

    std::vector < RegionType > m_InputRegions;

    TRANSFORM_TYPE m_InputTransformType, m_OutputTransformType;
};

} // end of namespace anima

// Include instantiation
#include "animaBaseTransformAgregator.hxx"