#include <tclap/CmdLine.h>

#include <itkDisplacementFieldJacobianDeterminantFilter.h>
#include <animaJacobianMatrixImageFilter.h>
#include <animaReadWriteFunctions.h>
#include <animaVelocityUtils.h>

int main(int ac, const char** av)
{    
    TCLAP::CmdLine cmd("INRIA / IRISA - VisAGeS Team", ' ', ANIMA_VERSION);
	
    TCLAP::ValueArg<std::string> inArg("i","input","Input field",true,"","input image",cmd);
    TCLAP::ValueArg<std::string> outArg("o","output","Output jacobian image",true,"","output image",cmd);
    
    TCLAP::ValueArg<unsigned int> neighArg("n","neigh","Neighborhood size for Jacobian computation (default: 1)",false,1,"neighborhood size",cmd);

    TCLAP::SwitchArg svfArg("S","svf","Compute the exponential of the input SVF",cmd,false);
    TCLAP::SwitchArg noIdArg("N","no-id","Do not add identity to the jacobian matrix",cmd,false);
    TCLAP::SwitchArg detArg("D","det","Simply compute the determinant of the jacobian (-N option ignored in that case)",cmd,false);
    TCLAP::ValueArg<unsigned int> nbpArg("p","numberofthreads","Number of threads to run on (default: all cores)",false,itk::MultiThreader::GetGlobalDefaultNumberOfThreads(),"number of threads",cmd);
    
    try
    {
        cmd.parse(ac,av);
    }
    catch (TCLAP::ArgException& e)
    {
        std::cerr << "Error: " << e.error() << "for argument " << e.argId() << std::endl;
        return(1);
    }

    const unsigned int Dimension = 3;
    typedef double PixelType;
    
    typedef itk::Image <itk::Vector <PixelType, Dimension>, Dimension> ImageType;

    ImageType::Pointer inputField = anima::readImage <ImageType> (inArg.getValue());
    
    if (svfArg.isSet())
    {
        typedef itk::StationaryVelocityFieldTransform <PixelType, Dimension> SVFType;
        SVFType::Pointer svfTrsf = SVFType::New();
        svfTrsf->SetParametersAsVectorField(inputField);

        typedef rpi::DisplacementFieldTransform <PixelType, Dimension> RPIDispType;
        RPIDispType::Pointer resTrsf = RPIDispType::New();

        anima::GetSVFExponential(svfTrsf.GetPointer(),resTrsf.GetPointer(),false);

        inputField = const_cast <ImageType *> (resTrsf->GetParametersAsVectorField());
    }

    if (detArg.isSet())
    {
        typedef itk::DisplacementFieldJacobianDeterminantFilter <ImageType, double> MainFilterType;
        MainFilterType::Pointer mainFilter = MainFilterType::New();

        mainFilter->SetInput(inputField);
        mainFilter->SetNumberOfThreads(nbpArg.getValue());
        mainFilter->SetUseImageSpacingOn();

        mainFilter->Update();

        anima::writeImage <MainFilterType::OutputImageType> (outArg.getValue(),mainFilter->GetOutput());
    }
    else
    {
        typedef anima::JacobianMatrixImageFilter <PixelType, PixelType, Dimension> MainFilterType;
        MainFilterType::Pointer mainFilter = MainFilterType::New();

        mainFilter->SetInput(inputField);
        mainFilter->SetNumberOfThreads(nbpArg.getValue());
        mainFilter->SetNeighborhood(neighArg.getValue());
        mainFilter->SetNoIdentity(noIdArg.isSet());

        mainFilter->Update();

        anima::writeImage <MainFilterType::OutputImageType> (outArg.getValue(),mainFilter->GetOutput());
    }

    return 0;
}
