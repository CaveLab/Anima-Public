#pragma once
#include "animaDistributionSampling.h"

#include <cmath>
#include <boost/math/distributions/beta.hpp>

#include <animaVectorOperations.h>
#include <animaLogarithmFunctions.h>
#include <animaBaseTensorTools.h>
#include <animaMatrixOperations.h>

#include <itkExceptionObject.h>

namespace anima
{

template <class T>
double SampleFromUniformDistribution(const T &a, const T &b, std::mt19937 &generator)
{
    // Define distribution U[a,b) [double values]
    std::uniform_real_distribution<T> uniDbl(a,b);
    return uniDbl(generator);
}

template <class VectorType>
void SampleFromUniformDistributionOn2Sphere(std::mt19937 &generator, VectorType &resVec)
{
    std::uniform_real_distribution<double> uniDbl(-1.0,1.0);
    double sqSum = 2;
    while (sqSum > 1)
    {
        resVec[0] = uniDbl(generator);
        resVec[1] = uniDbl(generator);

        sqSum = resVec[0] * resVec[0] + resVec[1] * resVec[1];
    }

    double factor = 2.0 * std::sqrt(1.0 - sqSum);
    resVec[0] *= factor;
    resVec[1] *= factor;
    resVec[2] = 2.0 * sqSum - 1.0;
}

template <class T>
unsigned int SampleFromBernoulliDistribution(const T &p, std::mt19937 &generator)
{
    std::bernoulli_distribution bernoulli(p);
    return bernoulli(generator);
}

template <class T>
double SampleFromGaussianDistribution(const T &mean, const T &std, std::mt19937 &generator)
{
    std::normal_distribution<T> normalDist(mean,std);
    return normalDist(generator);
}

template <class VectorType, class ScalarType>
void SampleFromMultivariateGaussianDistribution(const VectorType &mean, const vnl_matrix <ScalarType> &mat, VectorType &resVec,
                                                std::mt19937 &generator, bool isMatCovariance)
{
    unsigned int vectorSize = mat.rows();

    vnl_matrix <ScalarType> stdMatrix = mat;

    if (isMatCovariance)
    {
        vnl_matrix <ScalarType> eVecs(vectorSize,vectorSize);
        vnl_diag_matrix <ScalarType> eVals(vectorSize);

        itk::SymmetricEigenAnalysis < vnl_matrix <ScalarType>, vnl_diag_matrix<ScalarType>, vnl_matrix <ScalarType> > eigenComputer(vectorSize);
        eigenComputer.ComputeEigenValuesAndVectors(mat, eVals, eVecs);

        for (unsigned int i = 0;i < vectorSize;++i)
            eVals[i] = std::sqrt(eVals[i]);

        anima::RecomposeTensor(eVals,eVecs,stdMatrix);
    }

    VectorType tmpVec (resVec);
    for (unsigned int i = 0;i < vectorSize;++i)
        tmpVec[i] = SampleFromGaussianDistribution(0.0,1.0,generator);

    for (unsigned int i = 0;i < vectorSize;++i)
    {
        resVec[i] = mean[i];
        for (unsigned int j = 0;j < vectorSize;++j)
            resVec[i] += stdMatrix(i,j) * tmpVec[j];
    }
}

template <class VectorType, class ScalarType>
void SampleFromVMFDistribution(const ScalarType &kappa, const VectorType &meanDirection, VectorType &resVec, std::mt19937 &generator)
{
    VectorType tmpVec;

    for (unsigned int i = 0;i < 3;++i)
    {
        tmpVec[i] = 0;
        resVec[i] = 0;
    }

    // Code to compute rotation matrix from vectors, similar to registration code
    tmpVec[2] = 1;

    vnl_matrix <double> rotationMatrix = anima::GetRotationMatrixFromVectors(tmpVec,meanDirection).GetVnlMatrix();
    anima::TransformCartesianToSphericalCoordinates(meanDirection, tmpVec);

    if (std::abs(tmpVec[2] - 1.0) > 1.0e-6)
        throw itk::ExceptionObject(__FILE__, __LINE__,"Von Mises & Fisher sampling requires mean direction of norm 1.",ITK_LOCATION);

    double tmpVal = std::sqrt(kappa * kappa + 1.0);
    double b = (-2.0 * kappa + 2.0 * tmpVal) / 2.0;
    double a = (1.0 + kappa + tmpVal) / 2.0;
    double d = 4.0 * a * b / (1.0 + b) - 2.0 * anima::safe_log(2.0);

    boost::math::beta_distribution<double> betaDist(1.0, 1.0);

    double T = 1.0;
    double U = std::exp(d);
    double W = 0;

    while (2.0 * anima::safe_log(T) - T + d < anima::safe_log(U))
    {
        double Z = boost::math::quantile(betaDist, SampleFromUniformDistribution(0.0, 1.0, generator));
        U = SampleFromUniformDistribution(0.0, 1.0, generator);
        tmpVal = 1.0 - (1.0 - b) * Z;
        T = 2.0 * a * b / tmpVal;
        W = (1.0 - (1.0 + b) * Z) / tmpVal;
    }

    double theta = anima::SampleFromUniformDistribution(0.0, 2.0 * M_PI, generator);
    tmpVec[0] = std::sqrt(1.0 - W*W) * std::cos(theta);
    tmpVec[1] = std::sqrt(1.0 - W*W) * std::sin(theta);
    tmpVec[2] = W;

    for (unsigned int i = 0;i < 3;++i)
        for (unsigned int j = 0;j < 3;++j)
            resVec[i] += rotationMatrix(i,j) * tmpVec[j];
}

template <class VectorType, class ScalarType>
void SampleFromVMFDistributionNumericallyStable(const ScalarType &kappa, const VectorType &meanDirection, VectorType &resVec, std::mt19937 &generator)
{
    VectorType tmpVec;

    for (unsigned int i = 0;i < 3;++i)
    {
        tmpVec[i] = 0;
        resVec[i] = 0;
    }

    // Code to compute rotation matrix from vectors, similar to registration code
    tmpVec[2] = 1;

    vnl_matrix <double> rotationMatrix = anima::GetRotationMatrixFromVectors(tmpVec,meanDirection).GetVnlMatrix();
    anima::TransformCartesianToSphericalCoordinates(meanDirection, tmpVec);

    if (std::abs(tmpVec[2] - 1.0) > 1.0e-6)
        throw itk::ExceptionObject(__FILE__, __LINE__,"Von Mises & Fisher sampling requires mean direction of norm 1.",ITK_LOCATION);

    double xi = SampleFromUniformDistribution(0.0, 1.0, generator);
    double W = 1.0 + (anima::safe_log(xi) + anima::safe_log(1.0 - (xi - 1.0) * exp(-2.0 * kappa) / xi)) / kappa;
    double theta = SampleFromUniformDistribution(0.0, 2.0 * M_PI, generator);

    tmpVec[0] = std::sqrt(1.0 - W*W) * std::cos(theta);
    tmpVec[1] = std::sqrt(1.0 - W*W) * std::sin(theta);
    tmpVec[2] = W;

    for (unsigned int i = 0;i < 3;++i)
        for (unsigned int j = 0;j < 3;++j)
            resVec[i] += rotationMatrix(i,j) * tmpVec[j];
}

template <class ScalarType, class VectorType>
void
SampleFromWatsonDistribution(const ScalarType &kappa, const VectorType &meanDirection, VectorType &resVec, unsigned int DataDimension, std::mt19937 &generator)
{
    /**********************************************************************************************//**
         * \fn template <class ScalarType, class VectorType>
         * 	   void
         *     SampleFromWatsonDistribution(const ScalarType &kappa,
         * 					const VectorType &meanDirection,
         * 					VectorType &resVec,
         * 					unsigned int DataDimension,
         * 					std::mt19937 &generator)
         *
         * \brief	Sample from the Watson distribution using the procedure described in
         * 			Fisher et al., Statistical Analysis of Spherical Data, 1993, p.59.
         *
         * \author	Aymeric Stamm
         * \date	October 2013
         *
         * \param	kappa		   	Concentration parameter of the Watson distribution.
         * \param	meanDirection	Mean direction of the Watson distribution.
         * \param	resVec			Resulting sample.
         * \param	DataDimension	Dimension of the sphere + 1.
         * \param	generator		Pseudo-random number generator.
         **************************************************************************************************/

    VectorType tmpVec;

    for (unsigned int i = 0;i < DataDimension;++i)
    {
        tmpVec[i] = 0;
        resVec[i] = 0;
    }

    // Code to compute rotation matrix from vectors, taken from registration code
    tmpVec[2] = 1;
    
    VectorType rotationNormal;
    anima::ComputeCrossProduct(tmpVec, meanDirection, rotationNormal);
    anima::Normalize(rotationNormal, rotationNormal);
    
    // Now resuming onto sampling around direction [0,0,1]
    anima::TransformCartesianToSphericalCoordinates(meanDirection, tmpVec);
    
    double rotationAngle = tmpVec[0];
    
    if (std::abs(tmpVec[2] - 1.0) > 1.0e-6)
        throw itk::ExceptionObject(__FILE__, __LINE__,"The Watson distribution is on the 2-sphere.",ITK_LOCATION);

    double U, V, S;
    if (kappa > 1.0e-6) // Bipolar distribution
    {
        U = SampleFromUniformDistribution(0.0, 1.0, generator);
        S = 1.0 + std::log(U + (1.0 - U) * std::exp(-kappa)) / kappa;

        V = SampleFromUniformDistribution(0.0, 1.0, generator);

        if (V > 1.0e-6)
        {
            while (std::log(V) > kappa * S * (S - 1.0))
            {
                U = SampleFromUniformDistribution(0.0, 1.0, generator);
                S = 1.0 + std::log(U + (1.0 - U) * std::exp(-kappa)) / kappa;

                V = SampleFromUniformDistribution(0.0, 1.0, generator);

                if (V < 1.0e-6)
                    break;
            }
        }
    }
    else if (kappa < -1.0e-6) // Gridle distribution
    {
        double C1 = std::sqrt(std::abs(kappa));
        double C2 = std::atan(C1);
        U = SampleFromUniformDistribution(0.0, 1.0, generator);
        V = SampleFromUniformDistribution(0.0, 1.0, generator);
        S = (1.0 / C1) * std::tan(C2 * U);

        double T = kappa * S * S;
        while (V > (1.0 - T) * std::exp(T))
        {
            U = SampleFromUniformDistribution(0.0, 1.0, generator);
            V = SampleFromUniformDistribution(0.0, 1.0, generator);
            S = (1.0 / C1) * std::tan(C2 * U);
            T = kappa * S * S;
        }
    }
    else
    {
        // Sampling uniformly on the sphere
        S = std::cos(SampleFromUniformDistribution(0.0, M_PI, generator));
    }

    double phi = SampleFromUniformDistribution(0.0, 2.0 * M_PI, generator);

    tmpVec[0] = std::sqrt(1.0 - S*S) * std::cos(phi);
    tmpVec[1] = std::sqrt(1.0 - S*S) * std::sin(phi);
    tmpVec[2] = S;
    
    anima::RotateAroundAxis(tmpVec, rotationAngle, rotationNormal, resVec);
    
    double resNorm = anima::ComputeNorm(resVec);
    
    if (std::abs(resNorm - 1.0) > 1.0e-4)
    {
        std::cout << "Sampled direction norm: " << resNorm << std::endl;
        std::cout << "Mean direction: " << meanDirection << std::endl;
        std::cout << "Concentration parameter: " << kappa << std::endl;
        throw itk::ExceptionObject(__FILE__, __LINE__,"The Watson sampler should generate points on the 2-sphere.",ITK_LOCATION);
    }
    
    anima::Normalize(resVec,resVec);
}

template <class ScalarType, unsigned int DataDimension>
void
SampleFromWatsonDistribution(const ScalarType &kappa, const vnl_vector_fixed < ScalarType, DataDimension > &meanDirection, vnl_vector_fixed < ScalarType, DataDimension > &resVec, std::mt19937 &generator)
{
    /**********************************************************************************************//**
         * \fn template <class ScalarType, unsigned int DataDimension>
         * 	   void
         *     SampleFromWatsonDistribution(const ScalarType &kappa,
         * 					const vnl_vector_fixed < ScalarType, DataDimension > &meanDirection,
         * 					vnl_vector_fixed < ScalarType, DataDimension > &resVec,
         * 					std::mt19937 &generator)
         *
         * \brief	Sample from the Watson distribution using the procedure described in
         * 			Fisher et al., Statistical Analysis of Spherical Data, 1993, p.59.
         *
         * \author	Aymeric Stamm
         * \date	October 2013
         *
         * \param	kappa		   	Concentration parameter of the Watson distribution.
         * \param	meanDirection	Mean direction of the Watson distribution.
         * \param	resVec			Resulting sample.
         * \param	generator		Pseudo-random number generator.
         **************************************************************************************************/

    resVec.fill(0.0);
    SampleFromWatsonDistribution(kappa, meanDirection, resVec, DataDimension, generator);
}

template <class ScalarType, unsigned int DataDimension>
void
SampleFromWatsonDistribution(const ScalarType &kappa, const itk::Point < ScalarType, DataDimension > &meanDirection, itk::Point < ScalarType, DataDimension > &resVec, std::mt19937 &generator)
{
    /**********************************************************************************************//**
         * \fn template <class ScalarType, unsigned int DataDimension>
         * 	   void
         *     SampleFromWatsonDistribution(const ScalarType &kappa,
         * 					const itk::Point < ScalarType, DataDimension > &meanDirection,
         * 					itk::Point < ScalarType, DataDimension > &resVec,
         * 					std::mt19937 &generator)
         *
         * \brief	Sample from the Watson distribution using the procedure described in
         * 			Fisher et al., Statistical Analysis of Spherical Data, 1993, p.59.
         *
         * \author	Aymeric Stamm
         * \date	October 2013
         *
         * \param	kappa		   	Concentration parameter of the Watson distribution.
         * \param	meanDirection	Mean direction of the Watson distribution.
         * \param	resVec			Resulting sample.
         * \param	generator		Pseudo-random number generator.
         **************************************************************************************************/

    resVec.Fill(0.0);
    SampleFromWatsonDistribution(kappa, meanDirection, resVec, DataDimension, generator);
}

template <class ScalarType, unsigned int DataDimension>
void
SampleFromWatsonDistribution(const ScalarType &kappa, const itk::Vector < ScalarType, DataDimension > &meanDirection, itk::Vector < ScalarType, DataDimension > &resVec, std::mt19937 &generator)
{
    /**********************************************************************************************//**
         * \fn template <class ScalarType, unsigned int DataDimension>
         * 	   void
         *     SampleFromWatsonDistribution(const ScalarType &kappa,
         * 					const itk::Vector < ScalarType, DataDimension > &meanDirection,
         * 					itk::Vector < ScalarType, DataDimension > &resVec,
         * 					std::mt19937 &generator)
         *
         * \brief	Sample from the Watson distribution using the procedure described in
         * 			Fisher et al., Statistical Analysis of Spherical Data, 1993, p.59.
         *
         * \author	Aymeric Stamm
         * \date	October 2013
         *
         * \param	kappa		   	Concentration parameter of the Watson distribution.
         * \param	meanDirection	Mean direction of the Watson distribution.
         * \param	resVec			Resulting sample.
         * \param	generator		Pseudo-random number generator.
         **************************************************************************************************/

    resVec.Fill(0.0);
    SampleFromWatsonDistribution(kappa, meanDirection, resVec, DataDimension, generator);
}

} // end of namespace anima
