
// Qmitk
#include "DentalWidget.h"

// mitk image
#include <mitkImage.h>
#include <vtkConnectivityFilter.h>

#include "leastsquaresfit.h"
#include "mitkImageToSurfaceFilter.h"
#include "mitkNodePredicateDataType.h"
#include "mitkPointSet.h"
#include "mitkSurface.h"
#include "surfaceregistraion.h"

void DentalWidget::CheckUseSmoothing()
{
	if (m_Controls.lineEdit_smoothIteration->isEnabled())
	{
		m_Controls.lineEdit_smoothIteration->setDisabled(true);
		m_Controls.lineEdit_smoothRelaxtion->setDisabled(true);
	}
	else
	{
		m_Controls.lineEdit_smoothIteration->setEnabled(true);
		m_Controls.lineEdit_smoothRelaxtion->setEnabled(true);
	}
}

bool DentalWidget::ReconstructSurface()
{
	auto inputCtImage = dynamic_cast<mitk::Image*>(m_Controls.mitkNodeSelectWidget_intraopCt->GetSelectedNode()->GetData());

	if (inputCtImage != nullptr)
	{
		// The isosurface of the CT data into a single polydata
		double threshold = m_Controls.lineEdit_surfaceLowerBound->text().toDouble();
		auto mitkRecontructedSurfaces = mitk::Surface::New();
		mitk::ImageToSurfaceFilter::Pointer imageToSurfaceFilter = mitk::ImageToSurfaceFilter::New();

		// Set smoothing parameters
		if (m_Controls.checkBox_useSmooth->isChecked())
		{
			int iteration = m_Controls.lineEdit_smoothIteration->text().toInt();
			double relaxtion = m_Controls.lineEdit_smoothRelaxtion->text().toDouble();
			imageToSurfaceFilter->SmoothOn();
			imageToSurfaceFilter->SetSmoothRelaxation(relaxtion);
			imageToSurfaceFilter->SetSmoothIteration(iteration);
		}


		imageToSurfaceFilter->SetInput(inputCtImage);
		imageToSurfaceFilter->SetThreshold(threshold);
		mitkRecontructedSurfaces = imageToSurfaceFilter->GetOutput();
		

		// draw extracted surface
		auto nodeSteelballSurfaces = mitk::DataNode::New();
		nodeSteelballSurfaces->SetName("Reconstructed CBCT surface");
		nodeSteelballSurfaces->SetColor(1, 0.5, 0);
		// add new node
		nodeSteelballSurfaces->SetData(mitkRecontructedSurfaces);
		GetDataStorage()->Add(nodeSteelballSurfaces);
		return true;
	}
	return false;
}

bool DentalWidget::AutoReconstructSurface()
{
	if (GetDataStorage()->GetNamedNode("CBCT") == nullptr)
	{
		return false;
	}

	auto inputCtImage = dynamic_cast<mitk::Image*>(GetDataStorage()->GetNamedNode("CBCT")->GetData());

	if (inputCtImage != nullptr)
	{
		// The isosurface of the CT data into a single polydata
		double threshold = 1500;
		auto mitkRecontructedSurfaces = mitk::Surface::New();
		mitk::ImageToSurfaceFilter::Pointer imageToSurfaceFilter = mitk::ImageToSurfaceFilter::New();

		// Set smoothing parameters
		// if (m_Controls.checkBox_useSmooth->isChecked())
		// {
		// 	int iteration = m_Controls.lineEdit_smoothIteration->text().toInt();
		// 	double relaxtion = m_Controls.lineEdit_smoothRelaxtion->text().toDouble();
		// 	imageToSurfaceFilter->SmoothOn();
		// 	imageToSurfaceFilter->SetSmoothRelaxation(relaxtion);
		// 	imageToSurfaceFilter->SetSmoothIteration(iteration);
		// }


		imageToSurfaceFilter->SetInput(inputCtImage);
		imageToSurfaceFilter->SetThreshold(threshold);
		mitkRecontructedSurfaces = imageToSurfaceFilter->GetOutput();


		// draw extracted surface
		auto nodeSteelballSurfaces = mitk::DataNode::New();
		nodeSteelballSurfaces->SetName("Reconstructed CBCT surface");
		nodeSteelballSurfaces->SetColor(1, 0.5, 0);
		// add new node
		nodeSteelballSurfaces->SetData(mitkRecontructedSurfaces);
		GetDataStorage()->Add(nodeSteelballSurfaces);
		return true;
	}
	return false;
}

bool DentalWidget::GetLooseSteelballCenters()
{
	// INPUT 1: inputCtImage (MITK image)
	auto inputCtImage = dynamic_cast<mitk::Image*>(m_Controls.mitkNodeSelectWidget_intraopCt->GetSelectedNode()->GetData());


	// The isosurface of all steelballs as into a single polydata

	  // INPUT 2: voxelThreshold (double)
	double voxelThreshold = m_Controls.lineEdit_ballGrayValue->text().toDouble();
	auto mitkSteelBallSurfaces = mitk::Surface::New();
	mitk::ImageToSurfaceFilter::Pointer imageToSurfaceFilter = mitk::ImageToSurfaceFilter::New();

	imageToSurfaceFilter->SetInput(inputCtImage);
	imageToSurfaceFilter->SetThreshold(voxelThreshold);
	mitkSteelBallSurfaces = imageToSurfaceFilter->GetOutput();

	// Separate steelball surface by examining their connectivity
	vtkNew<vtkConnectivityFilter> vtkConnectivityFilter;
	vtkConnectivityFilter->SetInputData(mitkSteelBallSurfaces->GetVtkPolyData());

	vtkConnectivityFilter->SetExtractionModeToAllRegions();
	vtkConnectivityFilter->Update();
	int numberOfPotentialSteelBalls = vtkConnectivityFilter->GetNumberOfExtractedRegions();


	auto mitkSingleSteelballCenterPointset = mitk::PointSet::New(); // store each steelball's center
	double centerOfAllSteelballs[3]{ 0, 0, 0 };                       // the center of all steel balls

	// Iterate over all the potential steelball regions, extract the real ones by assessing their size (facets numbers)
	vtkConnectivityFilter->SetExtractionModeToSpecifiedRegions();
	for (int m = 0; m < numberOfPotentialSteelBalls; m++)
	{
		vtkConnectivityFilter->InitializeSpecifiedRegionList();
		vtkConnectivityFilter->AddSpecifiedRegion(m);
		vtkConnectivityFilter->Update();

		auto vtkSingleSteelBallSurface = vtkConnectivityFilter->GetPolyDataOutput();

		auto numberOfCells =
			vtkSingleSteelBallSurface->GetNumberOfCells(); // the total number of cells of a single mesh surface; each cell
														   // stores one facet of the mesh surface

		  // INPUT 3 & 4: facetNumberUpperThreshold (int) & facetNumberUpperThreshold (int)
		int facetNumberUpperThreshold = m_Controls.lineEdit_ballMaxCell->text().toInt();
		int facetNumberLowerThreshold = m_Controls.lineEdit_ballMinCell->text().toInt();

		if (numberOfCells < facetNumberUpperThreshold && numberOfCells > facetNumberLowerThreshold)
		{
			std::vector<double> inp_x(
				numberOfCells); // inp_x, inp_y and inp_z store one point of each facet on the mesh surface
			std::vector<double> inp_y(
				numberOfCells); // inp_x, inp_y and inp_z store one point of each facet on the mesh surface
			std::vector<double> inp_z(
				numberOfCells); // inp_x, inp_y and inp_z store one point of each facet on the mesh surface

			for (int n = 0; n < numberOfCells; n++)
			{
				auto tmpPoint = vtkSingleSteelBallSurface->GetCell(n)->GetPoints()->GetPoint(0);

				inp_x[n] = tmpPoint[0];
				inp_y[n] = tmpPoint[1];
				inp_z[n] = tmpPoint[2];
			}

			// use inp_x, inp_y and inp_z to simulate a sphere
			double cx, cy, cz;
			double R;

			lancetAlgorithm::fit_sphere(inp_x, inp_y, inp_z, cx, cy, cz, R);

			mitk::Point3D mitkTmpCenterPoint3D;
			mitkTmpCenterPoint3D[0] = cx;
			mitkTmpCenterPoint3D[1] = cy;
			mitkTmpCenterPoint3D[2] = cz;
			mitkSingleSteelballCenterPointset->InsertPoint(mitkTmpCenterPoint3D);

			centerOfAllSteelballs[0] = centerOfAllSteelballs[0] + cx;
			centerOfAllSteelballs[1] = centerOfAllSteelballs[1] + cy;
			centerOfAllSteelballs[2] = centerOfAllSteelballs[2] + cz;

			// // Draw simulated spheres
			// auto vtkBallSource0 = vtkSmartPointer<vtkSphereSource>::New();
			// vtkBallSource0->SetCenter(cx, cy, cz);
			// vtkBallSource0->SetRadius(R);
			// vtkBallSource0->Update();
			//
			// auto tmpNode = mitk::DataNode::New();
			//
			// tmpNode->SetName("Single steelball sphere");
			// auto mitkSteelBallSurfacesNew1 = mitk::Surface::New();
			// mitkSteelBallSurfacesNew1->SetVtkPolyData(vtkBallSource0->GetOutput());
			// tmpNode->SetData(mitkSteelBallSurfacesNew1);
			// // GetDataStorage()->Add(tmpNode);
		}


	}

	int numberOfActualSteelballs = mitkSingleSteelballCenterPointset->GetSize();

	centerOfAllSteelballs[0] = centerOfAllSteelballs[0] / numberOfActualSteelballs;
	centerOfAllSteelballs[1] = centerOfAllSteelballs[1] / numberOfActualSteelballs;
	centerOfAllSteelballs[2] = centerOfAllSteelballs[2] / numberOfActualSteelballs;

	// Sort the centers of the separate steelballs according to their distances to the group center
	std::vector<double> distancesToPointSetCenter(numberOfActualSteelballs);
	std::vector<int> distanceRanks(numberOfActualSteelballs);

	for (int i = 0; i < numberOfActualSteelballs; i++)
	{
		distancesToPointSetCenter[i] =
			sqrt(pow(centerOfAllSteelballs[0] - mitkSingleSteelballCenterPointset->GetPoint(i)[0], 2) +
				pow(centerOfAllSteelballs[1] - mitkSingleSteelballCenterPointset->GetPoint(i)[1], 2) +
				pow(centerOfAllSteelballs[2] - mitkSingleSteelballCenterPointset->GetPoint(i)[2], 2));

		distanceRanks[i] = i;
	}

	for (int i = 0; i < numberOfActualSteelballs; i++)
	{
		MITK_INFO << "Distance before sorting: " << distancesToPointSetCenter[i];
	}

	for (int i = 0; i < numberOfActualSteelballs - 2; i++)
	{
		for (int j = 0; j < numberOfActualSteelballs - 1 - i; j++)
		{
			double temp = 0;
			double temp2 = 0;
			if (distancesToPointSetCenter[j] > distancesToPointSetCenter[j + 1])
			{
				temp = distancesToPointSetCenter[j];
				distancesToPointSetCenter[j] = distancesToPointSetCenter[j + 1];
				distancesToPointSetCenter[j + 1] = temp;

				temp2 = distanceRanks[j];
				distanceRanks[j] = distanceRanks[j + 1];
				distanceRanks[j + 1] = temp2;
			}
		}
	}

	for (int i = 0; i < numberOfActualSteelballs; i++)
	{
		MITK_INFO << "Distance after sorting: " << distancesToPointSetCenter[i];
	}

	auto mitkSortedSingleSteelballCenterPointset = mitk::PointSet::New();
	for (int i = 0; i < numberOfActualSteelballs; i++)
	{
		mitkSortedSingleSteelballCenterPointset->InsertPoint(mitkSingleSteelballCenterPointset->GetPoint(distanceRanks[i]));
	}

	// // draw extracted  steel ball surfaces
	// auto nodeSteelballSurfaces = mitk::DataNode::New();
	// nodeSteelballSurfaces->SetName("Steelball surfaces");
	// // add new node
	// nodeSteelballSurfaces->SetData(mitkSteelBallSurfaces);
	// // GetDataStorage()->Add(nodeSteelballSurfaces);
	//
	// // add steel ball centers
	// auto nodeSteelballCenters = mitk::DataNode::New();
	// nodeSteelballCenters->SetName("Steelball centers");
	// // add new node
	// nodeSteelballCenters->SetData(mitkSingleSteelballCenterPointset);
	// // GetDataStorage()->Add(nodeSteelballCenters);



	  // OUTPUT: mitkSortedSingleSteelballCenterPointset (mitk::PointSet)

	// add sorted steel ball centers
	auto nodeSortedSteelballCenters = mitk::DataNode::New();
	nodeSortedSteelballCenters->SetName("Steelball centers");
	// add new node
	nodeSortedSteelballCenters->SetData(mitkSortedSingleSteelballCenterPointset);
	GetDataStorage()->Add(nodeSortedSteelballCenters);


	//TestEnhancedGetSteelballCenters();
	//AssembleSteelballCenters();

	return true;



}

double DentalWidget::GetPointDistance(const mitk::Point3D p0, const mitk::Point3D p1)
{
	double distance = sqrt(
		pow(p0[0] - p1[0], 2) +
		pow(p0[1] - p1[1], 2) +
		pow(p0[2] - p1[2], 2)
	);

	return distance;
}

void DentalWidget::TestEnhancedGetSteelballCenters()
{
	auto inputPointSet = dynamic_cast<mitk::PointSet*>(GetDataStorage()->GetNamedNode("Steelball centers")->GetData());
	int inputPoinSetNum = inputPointSet->GetSize();
	int lengthOfFingerPrint{ 6 };

	
	for(int i{0}; i < inputPoinSetNum; i++)
	{
		int metric{ 0 };
		
		for(int j{0}; j < lengthOfFingerPrint; j++)
		{
			bool metRequirement = false;
			double standardDistance = testFingerPrint[j];

			for(int k{0}; k < inputPoinSetNum; k++)
			{
				double tmpDistance = GetPointDistance(inputPointSet->GetPoint(i), inputPointSet->GetPoint(k));
				if(fabs(tmpDistance-standardDistance) < 0.4)
				{
					metRequirement = true;
					metric += 1;
					break;
				}
			}

			if(metRequirement == false)
			{
				break;
			}

		}

		m_Controls.textBrowser->append("metric: " + QString::number(metric));
		if(metric == lengthOfFingerPrint  )
		{
			// Add this point to the pointset
			auto tmpPointSet = mitk::PointSet::New();
			tmpPointSet->InsertPoint(inputPointSet->GetPoint(i));
			auto tmpNode = mitk::DataNode::New();
			tmpNode->SetName("Point P");
			tmpNode->SetData(tmpPointSet);
			GetDataStorage()->Add(tmpNode);
			break;
		}

	}

}

void DentalWidget::EnhancedGetSteelballCenters()
{
	auto inputPointSet = dynamic_cast<mitk::PointSet*>(GetDataStorage()->GetNamedNode("Steelball centers")->GetData());
	int inputPoinSetNum = inputPointSet->GetSize();
	auto steelballCenters = mitk::PointSet::New();

	int lengthOfFingerPrint{ 6 };
	int numOfTargetSteelballs{ 7 };


	for (int q{0}; q < numOfTargetSteelballs; q++)
	{
		double fingerPrint[6]
		{
			allBallFingerPrint[6 * q],
			allBallFingerPrint[6 * q + 1],
			allBallFingerPrint[6 * q + 2],
			allBallFingerPrint[6 * q + 3],
			allBallFingerPrint[6 * q + 4],
			allBallFingerPrint[6 * q + 5],
		};
		

		for (int i{ 0 }; i < inputPoinSetNum; i++)
		{
			int metric{ 0 };

			for (int j{ 0 }; j < lengthOfFingerPrint; j++)
			{
				bool metRequirement = false;
				double standardDistance = fingerPrint[j];

				for (int k{ 0 }; k < inputPoinSetNum; k++)
				{
					double tmpDistance = GetPointDistance(inputPointSet->GetPoint(i), inputPointSet->GetPoint(k));
					if (fabs(tmpDistance - standardDistance) < 0.4)
					{
						metRequirement = true;
						metric += 1;
						break;
					}
				}

				if (metRequirement == false)
				{
					break;
				}

			}

			//m_Controls.textBrowser->append("metric: " + QString::number(metric));
			if (metric == lengthOfFingerPrint)
			{

				// Add this point to the pointset
				steelballCenters->InsertPoint(inputPointSet->GetPoint(i));
				//break;
			}

		}

	}

	GetDataStorage()->Remove(GetDataStorage()->GetNamedNode("Steelball centers"));

	auto tmpNode = mitk::DataNode::New();
	tmpNode->SetName("Steelball centers");
	tmpNode->SetData(steelballCenters);
	GetDataStorage()->Add(tmpNode);

}


void DentalWidget::RemoveRedundantBalls()
{
	// EnhancedGetSteelballCenters();
	// EnhancedGetSteelballCenters();

	auto inputPointSet = dynamic_cast<mitk::PointSet*>(GetDataStorage()->GetNamedNode("Steelball centers")->GetData());

	auto tmpPointSet = mitk::PointSet::New();

	for(int i{0}; i < inputPointSet->GetSize(); i++)
	{
		bool unique = true;
		for(int j{0}; j < tmpPointSet->GetSize(); j++)
		{
			if(GetPointDistance(tmpPointSet->GetPoint(j),inputPointSet->GetPoint(i)) < 0.05)
			{
				unique = false;
				break;
			}
		}

		if(unique == true)
		{
			tmpPointSet->InsertPoint(inputPointSet->GetPoint(i));
		}
	}


	GetDataStorage()->Remove(GetDataStorage()->GetNamedNode("Steelball centers"));

	auto tmpNode = mitk::DataNode::New();
	tmpNode->SetName("Steelball centers");
	tmpNode->SetData(tmpPointSet);
	GetDataStorage()->Add(tmpNode);

}

void DentalWidget::GetSteelballCenters()
{
	m_Controls.textBrowser->append("------- Started steelball searching -------");
	if(GetDataStorage()->GetNamedNode("Steelball centers") != nullptr)
	{
		GetDataStorage()->Remove(GetDataStorage()->GetNamedNode("Steelball centers"));

	}

	GetLooseSteelballCenters();

	int oldNumOfCenters = dynamic_cast<mitk::PointSet*>(GetDataStorage()->GetNamedNode("Steelball centers")->GetData())->GetSize();

	EnhancedGetSteelballCenters();
	int newNumOfCenters = dynamic_cast<mitk::PointSet*>(GetDataStorage()->GetNamedNode("Steelball centers")->GetData())->GetSize();

	while (newNumOfCenters != oldNumOfCenters)
	{
		oldNumOfCenters = newNumOfCenters;
		EnhancedGetSteelballCenters();
		int newNumOfCenters = dynamic_cast<mitk::PointSet*>(GetDataStorage()->GetNamedNode("Steelball centers")->GetData())->GetSize();

	}

	RemoveRedundantBalls();

	auto standartSteelballCenters = mitk::PointSet::New();
	int stdCenterNum{ 7 };

	for(int i{0}; i < stdCenterNum; i++)
	{
		double tmpArray[3]
		{
			stdCenters[i * 3],
			stdCenters[i * 3 + 1],
			stdCenters[i * 3 + 2]
		};
		mitk::Point3D p(tmpArray);
		standartSteelballCenters->InsertPoint(p);
	}

	auto landmarkRegistrator = mitk::SurfaceRegistration::New();
	landmarkRegistrator->SetLandmarksSrc(standartSteelballCenters);
	landmarkRegistrator->SetLandmarksTarget(dynamic_cast<mitk::PointSet*>(GetDataStorage()->GetNamedNode("Steelball centers")->GetData()));

	landmarkRegistrator->ComputeLandMarkResult();
	double maxError = landmarkRegistrator->GetmaxLandmarkError();
	double avgError = landmarkRegistrator->GetavgLandmarkError();
	m_Controls.textBrowser->append("Maximal steelball center error: " + QString::number(maxError));
	m_Controls.textBrowser->append("Average steelball center error: " + QString::number(avgError));


	if (maxError < 1.5 && avgError < 1)
	{
		m_Controls.textBrowser->append("------- The correct centers have been found ! -------");
	}else
	{
		m_Controls.textBrowser->append("!!!!!! Warning: the found centers are highly problematic !!!!!!");
	}

}
