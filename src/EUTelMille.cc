// Authors:
// Philipp Roloff, DESY <mailto:philipp.roloff@desy.de>
// Joerg Behr, Hamburg Uni/DESY <joerg.behr@desy.de>
// Slava Libov, DESY <mailto:vladyslav.libov@desy.de>
// Igor Rubinskiy, DESY <mailto:igorrubinsky@gmail.com>
//
// Version: $Id$
/*
 *   This source code is part of the Eutelescope package of Marlin.
 *   You are free to use this source files for your own development as
 *   long as it stays in a public research context. You are not
 *   allowed to use it for commercial purpose. You must put this
 *   header with author names in all development based on this file.
 *
 */
// built only if GEAR and MARLINUTIL are used
#if defined(USE_GEAR) && defined(USE_MARLINUTIL)

// ROOT includes:
#include "TVector3.h"


// eutelescope includes ".h"
#include "EUTelMille.h"
#include "EUTelRunHeaderImpl.h"
#include "EUTelEventImpl.h"
#include "EUTELESCOPE.h"
#include "EUTelVirtualCluster.h"
#include "EUTelFFClusterImpl.h"
#include "EUTelDFFClusterImpl.h"
#include "EUTelBrickedClusterImpl.h"
#include "EUTelSparseClusterImpl.h"
#include "EUTelSparseCluster2Impl.h"
#include "EUTelExceptions.h"
#include "EUTelPStream.h"
#include "EUTelAlignmentConstant.h"
#include "EUTelReferenceHit.h"


// marlin includes ".h"
#include "marlin/Processor.h"
#include "marlin/Global.h"
#include "marlin/Exceptions.h"
#include "marlin/AIDAProcessor.h"

// marlin util includes
#include "mille/Mille.h"

// gear includes <.h>
#include <gear/GearMgr.h>
#include <gear/SiPlanesParameters.h>

// aida includes <.h>
#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)
#include <marlin/AIDAProcessor.h>
#include <AIDA/IHistogramFactory.h>
#include <AIDA/IHistogram1D.h>
#include <AIDA/ITree.h>
#endif

// lcio includes <.h>
#include <IO/LCWriter.h>
#include <UTIL/LCTime.h>
#include <EVENT/LCCollection.h>
#include <EVENT/LCEvent.h>
#include <IMPL/LCCollectionVec.h>
//#include <TrackerHitImpl2.h>
#include <IMPL/TrackerHitImpl.h>
#include <IMPL/TrackImpl.h>
#include <IMPL/LCFlagImpl.h>
#include <Exceptions.h>
#include <UTIL/CellIDEncoder.h>
#include <IMPL/TrackerRawDataImpl.h>
#include <IMPL/TrackerDataImpl.h>
#include <IMPL/TrackerPulseImpl.h>
#include <IMPL/LCCollectionVec.h>

// ROOT includes
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
#include <TRandom.h>
#include <TMinuit.h>
#include <TSystem.h>
#include <TMath.h>
#endif

// system includes <>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <memory>
#include <cmath>
#include <iostream>
#include <fstream>

using namespace std;
using namespace lcio;
using namespace marlin;
using namespace eutelescope;

#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
//evil global variables ...
//std::vector<EUTelMille::hit> hitsarray;
EUTelMille::hit *hitsarray;
unsigned int number_of_datapoints;
void fcn_wrapper(int &npar, double *gin, double &f, double *par, int iflag)
{
  EUTelMille::trackfitter fobj(hitsarray,number_of_datapoints);
  double p[4];
  p[0] = 0.0;
  p[1] = 0.0;
  p[2] = 0.0;
  p[3] = 0.0;
  f = fobj.fit(par,p);
}
#endif




// definition of static members mainly used to name histograms
#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)
std::string EUTelMille::_numberTracksLocalname   = "NumberTracks";
std::string EUTelMille::_chi2XLocalname          = "Chi2X";
std::string EUTelMille::_chi2YLocalname          = "Chi2Y";
std::string EUTelMille::_residualXLocalname      = "ResidualX";
std::string EUTelMille::_residualYLocalname      = "ResidualY";
std::string EUTelMille::_residualZLocalname      = "ResidualZ";

std::string EUTelMille::_residualXvsXLocalname      = "ResidualXvsX";
std::string EUTelMille::_residualXvsYLocalname      = "ResidualXvsY";
std::string EUTelMille::_residualYvsXLocalname      = "ResidualYvsX";
std::string EUTelMille::_residualYvsYLocalname      = "ResidualYvsY";
std::string EUTelMille::_residualZvsXLocalname      = "ResidualZvsX";
std::string EUTelMille::_residualZvsYLocalname      = "ResidualZvsY";


#endif




EUTelMille::EUTelMille () : Processor("EUTelMille") {

  //some default values
  FloatVec MinimalResidualsX;
  FloatVec MinimalResidualsY;
  FloatVec MaximalResidualsX;
  FloatVec MaximalResidualsY;

  FloatVec PedeUserStartValuesX;
  FloatVec PedeUserStartValuesY;

  FloatVec PedeUserStartValuesGamma;

  FloatVec SensorZPositions;

  FloatVec SensorXShifts;
  FloatVec SensorYShifts;

  FloatVec SensorGamma;

  FloatVec SensorAlpha;

  FloatVec SensorBeta;

  //maybe one has to chose a larger value than 6?
  for(int i =0; i<6;i++)
    {
      MinimalResidualsX.push_back(0.0);
      MinimalResidualsY.push_back(0.0);
      MaximalResidualsX.push_back(0.0);
      MaximalResidualsY.push_back(0.0);

      PedeUserStartValuesX.push_back(0.0);
      PedeUserStartValuesY.push_back(0.0);

      PedeUserStartValuesGamma.push_back(0.0);

      float zpos = 20000.0 +  20000.0 * (float)i;
      SensorZPositions.push_back(zpos);

      SensorXShifts.push_back(0.0);
      SensorYShifts.push_back(0.0);

      SensorGamma.push_back(0.0);
      SensorAlpha.push_back(0.0);
      SensorBeta.push_back(0.0);
    }



  // modify processor description
  _description =
    "EUTelMille uses the MILLE program to write data files for MILLEPEDE II.";

  // choose input mode
  registerOptionalParameter("InputMode","Selects the source of input hits."
                            "\n0 - hits read from hitfile with simple trackfinding. "
                            "\n1 - hits read from output of tracking processor. "
                            "\n2 - Test mode. Simple internal simulation and simple trackfinding. "
                            "\n3 - Mixture of a track collection from the telescope and hit collections for the DUT (only one DUT layer can be used unfortunately)",
                            _inputMode, static_cast <int> (0));

  registerOptionalParameter("AllowedMissingHits","Set how many hits (=planes) can be missing on a track candidate.",
                            _allowedMissingHits, static_cast <int> (0));

  registerOptionalParameter("MimosaClusterChargeMin","Select Mimosa26 clusters with charge above this value (default=1)",
                            _mimosa26ClusterChargeMin,  static_cast <int> (1) );


  // input collections
  std::vector<std::string > HitCollectionNameVecExample;
  HitCollectionNameVecExample.push_back("corrhits");

  registerInputCollections(LCIO::TRACKERHIT,"HitCollectionName",
                           "Hit collections name",
                           _hitCollectionName,HitCollectionNameVecExample);

  registerInputCollection(LCIO::TRACK,"TrackCollectionName",
                          "Track collection name",
                          _trackCollectionName,std::string("fittracks"));

  // parameters

  registerOptionalParameter("DistanceMax","Maximal allowed distance between hits entering the fit per 10 cm space between the planes.",
                            _distanceMax, static_cast <float> (2000.0));

  registerOptionalParameter("DistanceMaxVec","Maximal allowed distance between hits entering the fit per 10 cm space between the planes. One value for each neighbor planes. DistanceMax will be used for each pair if this vector is empty.",
                            _distanceMaxVec, std::vector<float> ());

  

  registerOptionalParameter("ExcludePlanes","Exclude planes from fit according to their sensor ids.",_excludePlanes_sensorIDs ,std::vector<int>());

  registerOptionalParameter("FixedPlanes","Fix sensor planes in the fit according to their sensor ids.",_FixedPlanes_sensorIDs ,std::vector<int>());


  registerOptionalParameter("MaxTrackCandidatesTotal","Maximal number of track candidates (Total).",_maxTrackCandidatesTotal, static_cast <int> (10000000));
  registerOptionalParameter("MaxTrackCandidates","Maximal number of track candidates.",_maxTrackCandidates, static_cast <int> (2000));

  registerOptionalParameter("BinaryFilename","Name of the Millepede binary file.",_binaryFilename, string ("mille.bin"));

  registerOptionalParameter("TelescopeResolution","Resolution of the telescope for Millepede (sigma_x=sigma_y.",_telescopeResolution, static_cast <float> (3.0));

  registerOptionalParameter("OnlySingleHitEvents","Use only events with one hit in every plane.",_onlySingleHitEvents, static_cast <int> (0));

  registerOptionalParameter("OnlySingleTrackEvents","Use only events with one track candidate.",_onlySingleTrackEvents, static_cast <int> (0));

  registerOptionalParameter("AlignMode","Number of alignment constants used. Available mode are: "
                            "\n1 - shifts in the X and Y directions and a rotation around the Z axis,"
                            "\n2 - only shifts in the X and Y directions"
                            "\n3 - (EXPERIMENTAL) shifts in the X,Y and Z directions and rotations around all three axis",
                            _alignMode, static_cast <int> (1));

  registerOptionalParameter("UseResidualCuts","Use cuts on the residuals to reduce the combinatorial background. 0 for off (default), 1 for on",_useResidualCuts,
                            static_cast <int> (0));

  registerOptionalParameter("AlignmentConstantLCIOFile","This is the name of the LCIO file name with the output alignment"
                            "constants (add .slcio)",_alignmentConstantLCIOFile, static_cast< string > ( "alignment.slcio" ) );

  registerOptionalParameter("AlignmentConstantCollectionName", "This is the name of the alignment collection to be saved into the slcio file",
                            _alignmentConstantCollectionName, static_cast< string > ( "alignment" ));

  registerOptionalParameter("ResidualsXMin","Minimal values of the hit residuals in the X direction for a track. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_residualsXMin,MinimalResidualsX);

  registerOptionalParameter("ResidualsYMin","Minimal values of the hit residuals in the Y direction for a track. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_residualsYMin,MinimalResidualsY);

  registerOptionalParameter("ResidualsXMax","Maximal values of the hit residuals in the X direction for a track. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_residualsXMax,MaximalResidualsX);

  registerOptionalParameter("ResidualsYMax","Maximal values of the hit residuals in the Y direction for a track. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_residualsYMax,MaximalResidualsY);



  registerOptionalParameter("ResolutionX","X resolution parameter for each plane. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_resolutionX,  std::vector<float> (static_cast <int> (6), 10.));

  registerOptionalParameter("ResolutionY","Y resolution parameter for each plane. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_resolutionY,std::vector<float> (static_cast <int> (6), 10.));

  registerOptionalParameter("ResolutionZ","Z resolution parameter for each plane. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_resolutionZ,std::vector<float> (static_cast <int> (6), 10.));

  registerOptionalParameter("ReferenceCollection","reference hit collection name ", _referenceHitCollectionName, static_cast <string> ("reference") );
 
  registerOptionalParameter("ApplyToReferenceCollection","Do you want the reference hit collection to be corrected by the shifts and tilts from the alignment collection? (default - false )",  _applyToReferenceHitCollection, static_cast< bool   > ( false ));
 

  registerOptionalParameter("FixParameter","Fixes the given alignment parameters in the fit if alignMode==3 is used. For each sensor an integer must be specified (If no value is given, then all parameters will be free). bit 0 = x shift, bit 1 = y shift, bit 2 = z shift, bit 3 = alpha, bit 4 = beta, bit 5 = gamma. Note: these numbers are ordered according to the z position of the sensors and NOT according to the sensor id.",_FixParameter, std::vector<int> (static_cast <int> (6), 24));

  registerOptionalParameter("GeneratePedeSteerfile","Generate a steering file for the pede program.",_generatePedeSteerfile, static_cast <int> (0));

  registerOptionalParameter("PedeSteerfileName","Name of the steering file for the pede program.",_pedeSteerfileName, string("steer_mille.txt"));

  registerOptionalParameter("RunPede","Execute the pede program using the generated steering file.",_runPede, static_cast <int> (0));

  registerOptionalParameter("UsePedeUserStartValues","Give start values for pede by hand (0 - automatic calculation of start values, 1 - start values defined by user).", _usePedeUserStartValues, static_cast <int> (0));

  registerOptionalParameter("PedeUserStartValuesX","Start values for the alignment for shifts in the X direction.",_pedeUserStartValuesX,PedeUserStartValuesX);

  registerOptionalParameter("PedeUserStartValuesY","Start values for the alignment for shifts in the Y direction.",_pedeUserStartValuesY,PedeUserStartValuesY);

  registerOptionalParameter("PedeUserStartValuesZ","Start values for the alignment for shifts in the Z direction.",_pedeUserStartValuesZ,std::vector<float> (static_cast <int> (6), 0.0));

  registerOptionalParameter("PedeUserStartValuesAlpha","Start values for the alignment for the angle alpha.",_pedeUserStartValuesAlpha,std::vector<float> (static_cast <int> (6), 0.0));
  
  registerOptionalParameter("PedeUserStartValuesBeta","Start values for the alignment for the angle beta.",_pedeUserStartValuesBeta,std::vector<float> (static_cast <int> (6), 0.0));
  
  registerOptionalParameter("PedeUserStartValuesGamma","Start values for the alignment for the angle gamma.",_pedeUserStartValuesGamma,PedeUserStartValuesGamma);

  registerOptionalParameter("TestModeSensorResolution","Resolution assumed for the sensors in test mode.",_testModeSensorResolution, static_cast <float> (3.0));

  registerOptionalParameter("TestModeXTrackSlope","Width of the track slope distribution in the x direction",_testModeXTrackSlope, static_cast <float> (0.0005));

  registerOptionalParameter("TestModeYTrackSlope","Width of the track slope distribution in the y direction",_testModeYTrackSlope, static_cast <float> (0.0005));

  registerOptionalParameter("TestModeSensorZPositions","Z positions of the sensors in test mode.",_testModeSensorZPositions,SensorZPositions);

  registerOptionalParameter("TestModeSensorXShifts","X shifts of the sensors in test mode (to be determined by the alignment).",
                            _testModeSensorXShifts,SensorXShifts);

  registerOptionalParameter("TestModeSensorYShifts","Y shifts of the sensors in test mode (to be determined by the alignment).",
                            _testModeSensorYShifts,SensorYShifts);


  registerOptionalParameter("TestModeSensorGamma","Rotation around the z axis of the sensors in test mode (to be determined by the alignment).",
                            _testModeSensorGamma,SensorGamma);


  registerOptionalParameter("TestModeSensorAlpha","Rotation around the x axis of the sensors in test mode (to be determined by the alignment).",
                            _testModeSensorAlpha,SensorAlpha);


  registerOptionalParameter("TestModeSensorBeta","Rotation around the y axis of the sensors in test mode (to be determined by the alignment).",
                            _testModeSensorBeta,SensorBeta);

  std::vector<int> initRect;
  registerOptionalParameter("UseSensorRectangular","Do not use all pixels for alignment, only these in the rectangular (A|B) e.g. (0,0) and (C|D) e.g. (100|100) of sensor S. Type in the way S1 A1 B1 C1 D1 S2 A2 B2 C2 D2 ...",
                            _useSensorRectangular,initRect);

  registerOptionalParameter("HotPixelCollectionName", "This is the name of the hot pixel collection to be saved into the output slcio file",
                             _hotPixelCollectionName, static_cast< string > ( "hotpixel_apix" ));

}

void EUTelMille::init() {
  // check if Marlin was built with GEAR support or not
#ifndef USE_GEAR

  streamlog_out ( ERROR2 ) << "Marlin was not built with GEAR support." << endl;
  streamlog_out ( ERROR2 ) << "You need to install GEAR and recompile Marlin with -DUSE_GEAR before continue." << endl;

  exit(-1);

#else

  // check if the GEAR manager pointer is not null!
  if ( Global::GEAR == 0x0 ) {
    streamlog_out ( ERROR2) << "The GearMgr is not available, for an unknown reason." << endl;
    exit(-1);
  }


#if !defined(USE_ROOT) && !defined(MARLIN_USE_ROOT)
  if(_alignMode == 3)
    {
      streamlog_out ( ERROR2) << "alignMode == 3 was chosen but Eutelescope was not build with ROOT support!" << endl;
      exit(-1);   
    }  
#endif

//
//  sensor-planes in geometry navigation:
//
  _siPlanesParameters  = const_cast<gear::SiPlanesParameters* > (&(Global::GEAR->getSiPlanesParameters()));
  _siPlanesLayerLayout = const_cast<gear::SiPlanesLayerLayout*> ( &(_siPlanesParameters->getSiPlanesLayerLayout() ));

  // clear the sensor ID vector
  _sensorIDVec.clear();

  // clear the sensor ID map
  _sensorIDVecMap.clear();
  _sensorIDtoZOrderMap.clear();

  // clear the sensor ID vector (z-axis order)
  _sensorIDVecZOrder.clear();

// copy-paste from another class (should be ideally part of GEAR!)
   double*   keepZPosition = new double[ _siPlanesLayerLayout->getNLayers() ];
   for ( int iPlane = 0 ; iPlane < _siPlanesLayerLayout->getNLayers(); iPlane++ ) 
   {
    int sensorID = _siPlanesLayerLayout->getID( iPlane );
        keepZPosition[ iPlane ] = _siPlanesLayerLayout->getLayerPositionZ(iPlane);

    _sensorIDVec.push_back( sensorID );
    _sensorIDVecMap.insert( make_pair( sensorID, iPlane ) );

    // count number of the sensors to the left of the current one:
    int _sensors_to_the_left = 0;
    for ( int jPlane = 0 ; jPlane < _siPlanesLayerLayout->getNLayers(); jPlane++ ) 
    {
        if( _siPlanesLayerLayout->getLayerPositionZ(jPlane) + 1e-06 <     keepZPosition[ iPlane ] )
        {
            _sensors_to_the_left++;
        }
    }
    streamlog_out (MESSAGE4) << " ";
    printf("iPlane %-3d sensor_#_along_Z_axis %-3d [z= %-9.3f ] [sensorID %-3d ]  \n", iPlane, _sensors_to_the_left,     keepZPosition[ iPlane ], sensorID);

    streamlog_out (MESSAGE4) << endl;

    _sensorIDVecZOrder.push_back( _sensors_to_the_left );
    _sensorIDtoZOrderMap.insert(make_pair( sensorID, _sensors_to_the_left));
   }


  _histogramSwitch = true;

  _referenceHitVec = 0;



  //lets guess the number of planes
  if(_inputMode == 0 || _inputMode == 2) 
    {

      // the number of planes is got from the GEAR description and is
      // the sum of the telescope reference planes and the DUT (if
      // any)
      _nPlanes = _siPlanesParameters->getSiPlanesNumber();
      if ( _siPlanesParameters->getSiPlanesType() == _siPlanesParameters->TelescopeWithDUT ) {
        ++_nPlanes;
      }

      if (_useSensorRectangular.size() == 0) {
	      streamlog_out(MESSAGE4) << "No rectangular limits on pixels of sensorplanes applied" << endl;
      } else {
	      if (_useSensorRectangular.size() % 5 != 0) {
		      streamlog_out(WARNING2) << "Wrong number of arguments in RectangularLimits! Ignoring this cut!" << endl;
	      } else {
		      streamlog_out(MESSAGE4) << "Reading in SensorRectangularCuts: " << endl;
		      int sensorcuts = _useSensorRectangular.size()/5;
		      for (int i = 0; i < sensorcuts; ++i) {
			      int sensor = _useSensorRectangular.at(5*i+0);
			      int A = _useSensorRectangular.at(5*i+1);
			      int B = _useSensorRectangular.at(5*i+2);
			      int C = _useSensorRectangular.at(5*i+3);
			      int D = _useSensorRectangular.at(5*i+4);
			      SensorRectangular r(sensor,A,B,C,D);
			      r.print();
			      _rect.addRectangular(r);
		      }
	      }
      } 

    }
  else if(_inputMode == 1)
    {
      _nPlanes = _siPlanesParameters->getSiPlanesNumber();
    }
  else if(_inputMode == 3)
    {

      // the number of planes is got from the GEAR description and is
      // the sum of the telescope reference planes and the DUT (if
      // any)
      _nPlanes = _siPlanesParameters->getSiPlanesNumber();
      if ( _siPlanesParameters->getSiPlanesType() == _siPlanesParameters->TelescopeWithDUT ) {
        ++_nPlanes;
      }
    }
  else
    {
      cout << "unknown input mode " << _inputMode << endl;
      exit(-1);
    }
  
  // an associative map for getting also the sensorID ordered
  map< double, int > sensorIDMap;
  //lets create an array with the z positions of each layer
  for ( int iPlane = 0 ; iPlane < _siPlanesLayerLayout->getNLayers(); iPlane++ ) {
    _siPlaneZPosition.push_back(_siPlanesLayerLayout->getLayerPositionZ(iPlane));
    sensorIDMap.insert( make_pair( _siPlanesLayerLayout->getLayerPositionZ(iPlane), _siPlanesLayerLayout->getID(iPlane) ) );
  }

  if  ( _siPlanesParameters->getSiPlanesType() == _siPlanesParameters->TelescopeWithDUT ) {
    _siPlaneZPosition.push_back(_siPlanesLayerLayout->getDUTPositionZ());
    sensorIDMap.insert( make_pair( _siPlanesLayerLayout->getDUTPositionZ(),  _siPlanesLayerLayout->getDUTID() ) ) ;
  }

  //lets sort the array with increasing z
  sort(_siPlaneZPosition.begin(), _siPlaneZPosition.end());

  
  //the user is giving sensor ids for the planes to be excluded. this
  //sensor ids have to be converted to a local index according to the
  //planes positions along the z axis.
  for (size_t i = 0; i < _FixedPlanes_sensorIDs.size(); i++)
    {
      map< double, int >::iterator iter = sensorIDMap.begin();
      int counter = 0;
      while ( iter != sensorIDMap.end() ) {
        if( iter->second == _FixedPlanes_sensorIDs[i])
          {
            _FixedPlanes.push_back(counter);
            break;
          }
        ++iter;
        ++counter;
      }
    }
  for (size_t i = 0; i < _excludePlanes_sensorIDs.size(); i++)
    {
      map< double, int >::iterator iter = sensorIDMap.begin();
      int counter = 0;
      while ( iter != sensorIDMap.end() ) {
        if( iter->second == _excludePlanes_sensorIDs[i])
          {
            printf("excludePlanes_sensorID %2d of %2d (%2d) \n", int(i), static_cast<int> (_excludePlanes_sensorIDs.size()) , counter );
            _excludePlanes.push_back(counter);
            break;
          }
        ++iter;
        ++counter;
      }
    }
  
  // strip from the map the sensor id already sorted.
  map< double, int >::iterator iter = sensorIDMap.begin();
  int counter = 0;
  while ( iter != sensorIDMap.end() ) {
    bool excluded = false;
    for (size_t i = 0; i < _excludePlanes.size(); i++)
      {
        if(_excludePlanes[i] == counter)
          {
            printf("excludePlanes %2d of %2d (%2d) \n", int(i), static_cast<int> (_excludePlanes_sensorIDs.size()), counter );
            excluded = true;
            break;
          }
      }
    if(!excluded)
      _orderedSensorID_wo_excluded.push_back( iter->second );
    _orderedSensorID.push_back( iter->second );

    ++iter;
    ++counter;
  }
  //


  //consistency
  if((int)_siPlaneZPosition.size() != _nPlanes)
    {
      streamlog_out ( ERROR2 ) << "the number of detected planes is " << _nPlanes << " but only " << _siPlaneZPosition.size() << " layer z positions were found!"  << endl;
      exit(-1);
    }

#endif



  // this method is called only once even when the rewind is active
  // usually a good idea to
  printParameters ();

  // set to zero the run and event counters
  _iRun = 0;
  _iEvt = 0;

  // Initialize number of excluded planes
  _nExcludePlanes = _excludePlanes.size();

  streamlog_out ( MESSAGE2 ) << "Number of planes excluded from the alignment fit: " << _nExcludePlanes << endl;

  // Initialise Mille statistics
  _nMilleDataPoints = 0;
  _nMilleTracks = 0;

  _waferResidX = new double[_nPlanes];
  _waferResidY = new double[_nPlanes];
  _waferResidZ = new double[_nPlanes];
  
  
  _xFitPos = new double[_nPlanes];
  _yFitPos = new double[_nPlanes];

  _telescopeResolX = new double[_nPlanes];
  _telescopeResolY = new double[_nPlanes];
  _telescopeResolZ = new double[_nPlanes];

//  printf("print resolution  X: %2d, Y: %2d, Z: %2d \n", _resolutionX.size(), _resolutionY.size(), _resolutionZ.size() );
  //check the consistency of the resolution parameters
  if(_alignMode == 3)
    {
       if( _resolutionX.size() != _resolutionY.size() )
       {
           throw InvalidParameterException("WARNING, length of resolution X and Y is not the same \n");
       }
       if( _resolutionY.size() != _resolutionZ.size() )
       {
           throw InvalidParameterException("WARNING, length of resolution Y and Z is not the same \n");
       }
                
      if(
         _resolutionX.size() != static_cast<unsigned int>(_nPlanes ) ||
         _resolutionY.size() != static_cast<unsigned int>(_nPlanes ) ||
         _resolutionZ.size() != static_cast<unsigned int>(_nPlanes )
         )
        {
//  ADDITIONAL PRINTOUTS   
         for(int i = 0; i < _resolutionX.size(); i++)
          { 
              printf("_resolutionX.at(%2d) = %8.3f; _resolutionY.at(%2d) = %8.3f;  _resolutionZ.at(%2d) = %8.3f;  \n", 
                      i,_resolutionX.at(i), i,_resolutionY.at(i), i,_resolutionZ.at(i) );
          }
          streamlog_out ( WARNING2 ) << "Consistency check of the resolution parameters failed. The array size is different than the number of found planes! The resolution parameters are set to default values now (see variable TelescopeResolution). This introduces a bias if the real values for X,Y and Z are rather different." << endl;
          _resolutionX.clear();
          _resolutionY.clear();
          _resolutionZ.clear();
          for(int i = 0; i < _nPlanes; i++)
            {
              _resolutionX.push_back(_telescopeResolution);
              _resolutionY.push_back(_telescopeResolution);
              _resolutionZ.push_back(_telescopeResolution);
            }
        }

//      printf("print FixParameter  \n");

      if(_FixParameter.size() != static_cast<unsigned int>(_nPlanes ) && _FixParameter.size() > 0)
        {
//  ADDITIONAL PRINTOUTS   
//       for(int i = 0; i < _FixParameter.size(); i++) printf(" _FixParameter(%2d) = %3d \n", i, _FixParameter.at(i) );
         
         streamlog_out ( WARNING2 ) << "Consistency check of the fixed parameters array failed. The array size is different than the number of found planes! The array is now set to default values, which means that all parameters are free in the fit." << endl;
          _FixParameter.clear();
          for(int i = 0; i < _nPlanes; i++)
            {
              _FixParameter.push_back(0);
            }
        }
      if(_FixParameter.size() == 0)
        {
          streamlog_out ( WARNING2 ) << "The fixed parameters array was found to be empty. It will be filled with default values. All parameters are free in the fit now." << endl;
          _FixParameter.clear();
          for(int i = 0; i < _nPlanes; i++)
            {
              _FixParameter.push_back(0);
            }
        }
    }

#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
  if(_alignMode == 3)
    {
//       number_of_datapoints = _nPlanes -_nExcludePlanes;
      number_of_datapoints = _nPlanes;
      hitsarray = new hit[number_of_datapoints];
    }
#endif

  // booking histograms
  bookHistos();

  streamlog_out ( MESSAGE2 ) << "Initialising Mille..." << endl;
  _mille = new Mille(_binaryFilename.c_str());
  streamlog_out ( MESSAGE2 ) << "The filename for the binary file is: " << _binaryFilename.c_str() << endl;

  for(int i = 0; i < _maxTrackCandidates; i++)
    {
      _xPos.push_back(std::vector<double>(_nPlanes,0.0));
      _yPos.push_back(std::vector<double>(_nPlanes,0.0));
      _zPos.push_back(std::vector<double>(_nPlanes,0.0));
 
//      _xPosTrack.push_back(std::vector<double>(_nPlanes,0.0));
//      _yPosTrack.push_back(std::vector<double>(_nPlanes,0.0));
//      _zPosTrack.push_back(std::vector<double>(_nPlanes,0.0));
    }

  if(_distanceMaxVec.size() > 0)
    {
      if(_distanceMaxVec.size() !=  static_cast<unsigned int>(_nPlanes ) )
        {
          streamlog_out ( WARNING2 ) << "Consistency check of the DistanceMaxVec array failed. Its size is different compared to the number of planes! Will now use _distanceMax for each pair of planes." << endl;
          _distanceMaxVec.clear();
          for(int i = 0; i < _nPlanes-1; i++)
            {
              _distanceMaxVec.push_back(_distanceMax);
            }
        }
    }
  else
    {
      _distanceMaxVec.clear();
      for(int i = 0; i < _nPlanes-1; i++)
        {
          _distanceMaxVec.push_back(_distanceMax);
        }
    }

/*
//prepare a planes center points and normal vectors:
  for(int i=0; i < _orderedSensorID.size(); i++)
  {
	// this is a normal vector to the plane
	TVectorD	NormalVector(3);
	NormalVector(0) = 0.;
	NormalVector(1) = 0.;
	NormalVector(2) = 1.; //along z axis !! {true but it's can different direction for the local sensor frame z !!}

	TVectorD	CenterVector(3);
	NormalVector(0) = 0.;
	NormalVector(1) = 0.;
	NormalVector(2) = 0.;
  } 
*/

  streamlog_out ( MESSAGE2 ) << "end of init" << endl;
}

void EUTelMille::processRunHeader (LCRunHeader * rdr) {

  auto_ptr<EUTelRunHeaderImpl> header ( new EUTelRunHeaderImpl (rdr) );
  header->addProcessor( type() ) ;

  // this is the right place also to check the geometry ID. This is a
  // unique number identifying each different geometry used at the
  // beam test. The same number should be saved in the run header and
  // in the xml file. If the numbers are different, instead of barely
  // quitting ask the user what to do.

  if ( header->getGeoID() != _siPlanesParameters->getSiPlanesID() ) {
    streamlog_out ( ERROR2 ) << "Error during the geometry consistency check: " << endl;
    streamlog_out ( ERROR2 ) << "The run header says the GeoID is " << header->getGeoID() << endl;
    streamlog_out ( ERROR2 ) << "The GEAR description says is     " << _siPlanesParameters->getSiPlanesNumber() << endl;

#ifdef EUTEL_INTERACTIVE
    string answer;
    while (true) {
      streamlog_out ( ERROR2 ) << "Type Q to quit now or C to continue using the actual GEAR description anyway [Q/C]" << endl;
      cin >> answer;
      // put the answer in lower case before making the comparison.
      transform( answer.begin(), answer.end(), answer.begin(), ::tolower );
      if ( answer == "q" ) {
        exit(-1);
      } else if ( answer == "c" ) {
        break;
      }
    }
#endif

  }

  // increment the run counter
  ++_iRun;
}



void EUTelMille::findtracks2(
                            int missinghits,
                            std::vector<std::vector<int> > &indexarray,
                            std::vector<int> vec,
                            std::vector<std::vector<EUTelMille::HitsInPlane> > &_allHitsArray,
                            int i,
                            int y
                            )
{
 if(y==-1) missinghits++;

 if( missinghits > getAllowedMissingHits() ) 
 {
//   printf("too mmany missing hits %5d\n", missinghits); 
   // recursive chain is dropped here;
   return;
 }

 //  ADDITIONAL PRINTOUTS   
// printf("come to PLANE id=%3d y=%3d  : : _allHitsArray[i].size()=%5d \n", i,y, _allHitsArray[i].size() );

 if(i>0)
 { 
    vec.push_back(y); // recall hit id from the plane (i-1)
 }

 if( (_allHitsArray[i].size() == 0) && (i <_allHitsArray.size()-1) )
 {
    findtracks2(missinghits,indexarray,vec, _allHitsArray, i+1, -1 ); 
//    return; // stop here
 } 

 for(size_t j =0; j < _allHitsArray[i].size(); j++)
    {
      int ihit = (int)j;
 //     printf("plane %2d of %2d ; hit %2d \n", i, (int)(_allHitsArray.size())-1, ihit);

      //if we are not in the last plane, call this method again
      if(i<(int)(_allHitsArray.size())-1)
        {
          vec.push_back( ihit); //index of the cluster in the last plane
         
          //track candidate requirements
          bool taketrack = true;
          const int e = vec.size()-2;
          if(e >= 0)
            {
              double residualX  = -999999.;
              double residualY  = -999999.;
              double residualZ  = -999999.;

              // ACTIVE
              // stop on the last non-zero hit
//              for(int ivec=0; ivec<=e; ivec++)

              // now loop through all hits on a track candidate "vec"
              // start at the end, stop on the first non-zero hit
              for(int ivec=e; ivec>=e; --ivec)                  //     <-> OFF
              {
                if(vec[ivec]>=0) // non zero hit has id vec[ivec]>=0 {otherwise -1}
                {
                  double x = _allHitsArray[ivec][vec[ivec]].measuredX;
                  double y = _allHitsArray[ivec][vec[ivec]].measuredY;
                  double z = _allHitsArray[ivec][vec[ivec]].measuredZ;
//                  printf("e %2d ivec %2d of %2d, vec[%2d]=%2d at [%5.2f %5.2f %8.2f] \n", e, ivec, vec.size(), ivec,vec[ivec], x,y,z  );
                  residualX  = abs(x - _allHitsArray[e+1][vec[e+1]].measuredX);
                  residualY  = abs(y - _allHitsArray[e+1][vec[e+1]].measuredY);
                  residualZ  = abs(z - _allHitsArray[e+1][vec[e+1]].measuredZ);
//                  printf("e %2d ivec %2d of %2d, vec[%2d]=%2d at [%5.2f %5.2f %8.2f] dist[%8.2f %8.2f %8.2f]\n", 
//                            e+1, ivec, vec.size(), ivec,vec[ivec], x,y,z, distanceX,distanceY,distanceZ );
                  break; 
                }   
              }
           
//              const double dM = _distanceMaxVec[e];
/*             
              double distancemax = dM * ( distanceZ  / 100000.0);

              if( distanceX >= distancemax )
                taketrack = false;

              if( distanceY >= distancemax )
                taketrack = false;
*/
             if ( 
                   residualX < _residualsXMin[e] || residualX > _residualsXMax[e] ||
                   residualY < _residualsYMin[e] || residualY > _residualsYMax[e] 
                 )
                 taketrack = false;
              
/* 
             if( distanceX >= dM )
                taketrack = false;

              if( distanceY >= dM )
                taketrack = false;
*/

              if( taketrack == false )
              {
                taketrack = true; 
                ihit=-1;
              } 
 //             printf("distanceX:%5.2f  distanceY:%5.2f  distancemax:%5.2f  taketrack:%1d\n", distanceX, distanceY, dM, taketrack);
            }
          vec.pop_back(); 

//  ADDITIONAL PRINTOUTS   
          if(taketrack)
          { 
/*
              for(int ivec = 0; ivec< vec.size() ; ivec++)
              {       
                if(vec[ivec]>=0) // non zero hit has id vec[ivec]>=0 {otherwise -1}
                {
                  double x = _allHitsArray[ivec][vec[ivec]].measuredX;
                  double y = _allHitsArray[ivec][vec[ivec]].measuredY;
                  double z = _allHitsArray[ivec][vec[ivec]].measuredZ;
                  printf("come to the next plane from id=%3d clu=%3d of %3d at[%5.2f %5.2f %13.2f]\n", ivec, vec[ivec], _allHitsArray[ivec].size(), x,y,z);
                }
                else
                { 
                   printf("come to the next plane from id=%3d clu=%3d \n", ivec, vec[ivec]);
                }
              }
*/
              findtracks2(missinghits, indexarray, vec, _allHitsArray, i+1, ihit );
          }
        }
      else
        {
          //we are in the last plane
          vec.push_back( ihit ); //index of the cluster in the last plane

          //track candidate requirements
          bool taketrack = true;
          const int e = vec.size()-2;
          if(e >= 0)
            {
              double residualX  = -999999.;
              double residualY  = -999999.;
              double residualZ  = -999999.;

              // ACTIVE:
              // stop on the last non-zero hit
//              for(int ivec=0; ivec<=e; ivec++)

              // now loop through all hits on a track candidate "vec"
              // start at the end, stop on the first non-zero hit
              for(int ivec=e; ivec>=e; --ivec)                        //   <-> OFF
              {
                if(vec[ivec]>=0) // non zero hit has id vec[ivec]>=0 {otherwise -1}
                {
                  double x = _allHitsArray[ivec][vec[ivec]].measuredX;
                  double y = _allHitsArray[ivec][vec[ivec]].measuredY;
                  double z = _allHitsArray[ivec][vec[ivec]].measuredZ;
//                  printf("e %2d ivec %2d of %2d, vec[%2d]=%2d at [%5.2f %5.2f %8.2f] \n", e, ivec, vec.size(), ivec,vec[ivec], x,y,z  );
                  residualX  = abs(x - _allHitsArray[e+1][vec[e+1]].measuredX);
                  residualY  = abs(y - _allHitsArray[e+1][vec[e+1]].measuredY);
                  residualZ  = abs(z - _allHitsArray[e+1][vec[e+1]].measuredZ);
//                  printf("e %2d ivec %2d of %2d, vec[%2d]=%2d at [%5.2f %5.2f %8.2f] dist[%8.2f %8.2f %8.2f]\n", 
//                            e+1, ivec, vec.size(), ivec,vec[ivec], x,y,z, distanceX,distanceY,distanceZ );
                  break; 
                }   
              }
           
//              const double dM = _distanceMaxVec[e];
/*
              double distancemax = dM * ( distanceZ  / 100000.0);

              if( distanceX >= distancemax )
                taketrack = false;

              if( distanceY >= distancemax )
                taketrack = false;
*/
/*
              if( distanceX >= dM )
                taketrack = false;

              if( distanceY >= dM )
                taketrack = false;
*/
              if ( 
                   residualX < _residualsXMin[e] || residualX > _residualsXMax[e] ||
                   residualY < _residualsYMin[e] || residualY > _residualsYMax[e] 
                 )
                 taketrack = false;
 
//              printf("distanceX:%5.2f  distanceY:%5.2f  distancemax:%5.2f  \n", distanceX, distanceY, dM);
 
              if( taketrack == false )
              {
                taketrack = true; 
                ihit=-1;
              } 
            }

          if((int)indexarray.size() >= _maxTrackCandidates)
            taketrack = false;
//  ADDITIONAL PRINTOUTS   
//          printf("come to the last plane id=%3d clu=%3d of %3d ; vec.size=%5d\n", i, j, _allHitsArray[i].size(), vec.size());
 
          if(taketrack)
            {
/*
               for(int ivec = 0; ivec< vec.size() ; ivec++)
               {       
                if(vec[ivec]>=0) // non zero hit has id vec[ivec]>=0 {otherwise -1}
                {
                  double x = _allHitsArray[ivec][vec[ivec]].measuredX;
                  double y = _allHitsArray[ivec][vec[ivec]].measuredY;
                  double z = _allHitsArray[ivec][vec[ivec]].measuredZ;
                  printf("come to the LAST plane from id=%3d clu=%3d of %3d at[%5.2f %5.2f %13.2f]\n", ivec, vec[ivec], _allHitsArray[ivec].size(), x,y,z);
                }
                else
                { 
                   printf("come to the LAST plane from id=%3d clu=%3d \n", ivec, vec[ivec]);
                }
               }
*/
               indexarray.push_back(vec);
            }
          vec.pop_back(); //last element must be removed because the
                          //vector is still used -> we are in a last plane hit loop!

        }
    }
}



void EUTelMille::findtracks(
                            std::vector<std::vector<int> > &indexarray,
                            std::vector<int> vec,
                            std::vector<std::vector<EUTelMille::HitsInPlane> > &_hitsArray,
                            int i,
                            int y
                            )
{
 if(i>0)
    vec.push_back(y);

//  ADDITIONAL PRINTOUTS   
// printf("come to the  plane id=%3d y=%3d  : : _hitsArray[i].size()=%5d \n", i,y, _hitsArray[i].size() );
 for(size_t j =0; j < _hitsArray[i].size(); j++)
    {
      //if we are not in the last plane, call this method again
      if(i<(int)(_hitsArray.size())-1)
        {
          vec.push_back((int)j); //index of the cluster in the last plane
         
          //track candidate requirements
          bool taketrack = true;
          const int e = vec.size()-2;
          if(e >= 0)
            {
              double distance = sqrt(
                                     pow( _hitsArray[e][vec[e]].measuredX - _hitsArray[e+1][vec[e+1]].measuredX ,2) +
                                     pow( _hitsArray[e][vec[e]].measuredY - _hitsArray[e+1][vec[e+1]].measuredY ,2)
                                     );
              double distance_z = _hitsArray[e+1][vec[e+1]].measuredZ - _hitsArray[e][vec[e]].measuredZ;
              
              
              const double dM = _distanceMaxVec[e];
              
              double distancemax = dM * ( distance_z / 100000.0);
              
              if( distance >= distancemax )
                taketrack = false;
              
              if(_onlySingleHitEvents == 1 && (_hitsArray[e].size() != 1 || _hitsArray[e+1].size() != 1))
                taketrack = false;
            }
          vec.pop_back(); 

//  ADDITIONAL PRINTOUTS   
//          printf("come to the next plane id=%3d clu=%3d of %3d \n", i, j, _hitsArray[i].size());
          if(taketrack)
            findtracks(indexarray,vec, _hitsArray, i+1,(int)j);
        }
      else
        {
          //we are in the last plane
          vec.push_back((int)j); //index of the cluster in the last plane

          //track candidate requirements
          bool taketrack = true;
          for(size_t e =0; e < vec.size()-1; e++)
            {
              double distance = sqrt(
                                     pow( _hitsArray[e][vec[e]].measuredX - _hitsArray[e+1][vec[e+1]].measuredX ,2) +
                                     pow( _hitsArray[e][vec[e]].measuredY - _hitsArray[e+1][vec[e+1]].measuredY ,2)
                                     );
              double distance_z = _hitsArray[e+1][vec[e+1]].measuredZ - _hitsArray[e][vec[e]].measuredZ;
             
              const double dM = _distanceMaxVec[e];
             
              double distancemax = dM * ( distance_z / 100000.0);

              if( distance >= distancemax )
                taketrack = false;

              if(_onlySingleHitEvents == 1 && (_hitsArray[e].size() != 1 || _hitsArray[e+1].size() != 1))
                taketrack = false;

            }
          if((int)indexarray.size() >= _maxTrackCandidates)
            taketrack = false;
//  ADDITIONAL PRINTOUTS   
//          printf("come to the last plane id=%3d clu=%3d of %3d ; vec.size=%5d\n", i, j, _hitsArray[i].size(), vec.size());
 
          if(taketrack)
            {
              indexarray.push_back(vec);
            }
          vec.pop_back(); //last element must be removed because the
                          //vector is still used
        }
    }
}


void EUTelMille::FitTrack(int nPlanesFitter, double xPosFitter[], double yPosFitter[], double zPosFitter[], double xResFitter[], double yResFitter[], double chi2Fit[2], double residXFit[], double residYFit[], double angleFit[2]) {

  int sizearray;

  if (_nExcludePlanes > 0) {
    sizearray = nPlanesFitter - _nExcludePlanes;
  } else {
    sizearray = nPlanesFitter;
  }

  double * xPosFit = new double[sizearray];
  double * yPosFit = new double[sizearray];
  double * zPosFit = new double[sizearray];
  double * xResFit = new double[sizearray];
  double * yResFit = new double[sizearray];

  int nPlanesFit = 0;

  for (int help = 0; help < nPlanesFitter; help++) {

    int excluded = 0;

    // check if actual plane is excluded
    if (_nExcludePlanes > 0) {
      for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
        if (help == _excludePlanes[helphelp]) {
          excluded = 1;
        }
//        printf("735: excludePlanes %2d of %2d (%2d) \n", helphelp, _nExcludePlanes, excluded );
      }
    }

    if (excluded == 1) {
      // do noting
    } else {
      xPosFit[nPlanesFit] = xPosFitter[help];
      yPosFit[nPlanesFit] = yPosFitter[help];
      zPosFit[nPlanesFit] = zPosFitter[help];
      xResFit[nPlanesFit] = xResFitter[help];
      yResFit[nPlanesFit] = yResFitter[help];
      nPlanesFit++;
    }
  }

  // ++++++++++++++++++++++++++++++++++++++++++++++++++++++
  // ++++++++++++ See Blobel Page 226 !!! +++++++++++++++++
  // ++++++++++++++++++++++++++++++++++++++++++++++++++++++

  int counter;

  float S1[2]   = {0,0};
  float Sx[2]   = {0,0};
  float Xbar[2] = {0,0};

  float * Zbar_X = new float[nPlanesFit];
  float * Zbar_Y = new float[nPlanesFit];
  for (counter = 0; counter < nPlanesFit; counter++){
    Zbar_X[counter] = 0.;
    Zbar_Y[counter] = 0.;
  }

  float Sy[2]     = {0,0};
  float Ybar[2]   = {0,0};
  float Sxybar[2] = {0,0};
  float Sxxbar[2] = {0,0};
  float A2[2]     = {0,0};

  // define S1
  for( counter = 0; counter < nPlanesFit; counter++ ){
    S1[0] = S1[0] + 1/pow(xResFit[counter],2);
    S1[1] = S1[1] + 1/pow(yResFit[counter],2);
  }

  // define Sx
  for( counter = 0; counter < nPlanesFit; counter++ ){
    Sx[0] = Sx[0] + zPosFit[counter]/pow(xResFit[counter],2);
    Sx[1] = Sx[1] + zPosFit[counter]/pow(yResFit[counter],2);
  }

  // define Xbar
  Xbar[0]=Sx[0]/S1[0];
  Xbar[1]=Sx[1]/S1[1];

  // coordinate transformation !! -> bar
  for( counter = 0; counter < nPlanesFit; counter++ ){
    Zbar_X[counter] = zPosFit[counter]-Xbar[0];
    Zbar_Y[counter] = zPosFit[counter]-Xbar[1];
  }

  // define Sy
  for( counter = 0; counter < nPlanesFit; counter++ ){
    Sy[0] = Sy[0] + xPosFit[counter]/pow(xResFit[counter],2);
    Sy[1] = Sy[1] + yPosFit[counter]/pow(yResFit[counter],2);
  }

  // define Ybar
  Ybar[0]=Sy[0]/S1[0];
  Ybar[1]=Sy[1]/S1[1];

  // define Sxybar
  for( counter = 0; counter < nPlanesFit; counter++ ){
    Sxybar[0] = Sxybar[0] + Zbar_X[counter] * xPosFit[counter]/pow(xResFit[counter],2);
    Sxybar[1] = Sxybar[1] + Zbar_Y[counter] * yPosFit[counter]/pow(yResFit[counter],2);
  }

  // define Sxxbar
  for( counter = 0; counter < nPlanesFit; counter++ ){
    Sxxbar[0] = Sxxbar[0] + Zbar_X[counter] * Zbar_X[counter]/pow(xResFit[counter],2);
    Sxxbar[1] = Sxxbar[1] + Zbar_Y[counter] * Zbar_Y[counter]/pow(yResFit[counter],2);
  }

  // define A2
  A2[0]=Sxybar[0]/Sxxbar[0];
  A2[1]=Sxybar[1]/Sxxbar[1];

  // Calculate chi sqaured
  // Chi^2 for X and Y coordinate for hits in all planes
  for( counter = 0; counter < nPlanesFit; counter++ ){
    chi2Fit[0] += pow(-zPosFit[counter]*A2[0]
                      +xPosFit[counter]-Ybar[0]+Xbar[0]*A2[0],2)/pow(xResFit[counter],2);
    chi2Fit[1] += pow(-zPosFit[counter]*A2[1]
                      +yPosFit[counter]-Ybar[1]+Xbar[1]*A2[1],2)/pow(yResFit[counter],2);
  }

  for( counter = 0; counter < nPlanesFitter; counter++ ) {
    residXFit[counter] = (Ybar[0]-Xbar[0]*A2[0]+zPosFitter[counter]*A2[0])-xPosFitter[counter];
    residYFit[counter] = (Ybar[1]-Xbar[1]*A2[1]+zPosFitter[counter]*A2[1])-yPosFitter[counter];

    // residXFit[counter] = xPosFitter[counter] - (Ybar[0]-Xbar[0]*A2[0]+zPosFitter[counter]*A2[0]);
    // residYFit[counter] = yPosFitter[counter] - (Ybar[1]-Xbar[1]*A2[1]+zPosFitter[counter]*A2[1]);

  }

  // define angle
  angleFit[0] = atan(A2[0]);
  angleFit[1] = atan(A2[1]);

  // clean up
  delete [] zPosFit;
  delete [] yPosFit;
  delete [] xPosFit;
  delete [] yResFit;
  delete [] xResFit;

  delete [] Zbar_X;
  delete [] Zbar_Y;

}

void  EUTelMille::FillHotPixelMap(LCEvent *event)
{
    LCCollectionVec *hotPixelCollectionVec = 0;
    try 
    {
      hotPixelCollectionVec = static_cast< LCCollectionVec* > ( event->getCollection( _hotPixelCollectionName  ) );
//      streamlog_out ( MESSAGE ) << "_hotPixelCollectionName " << _hotPixelCollectionName.c_str() << " found" << endl; 
    }
    catch (...)
    {
      streamlog_out ( MESSAGE ) << "_hotPixelCollectionName " << _hotPixelCollectionName.c_str() << " not found" << endl; 
      return;
    }

        CellIDDecoder<TrackerDataImpl> cellDecoder( hotPixelCollectionVec );
// 	CellIDDecoder<TrackerDataImpl> cellDecoder( zsInputCollectionVec );
	
        for(int i=0; i<  hotPixelCollectionVec->getNumberOfElements(); i++)
        {
           TrackerDataImpl* hotPixelData = dynamic_cast< TrackerDataImpl *> ( hotPixelCollectionVec->getElementAt( i ) );
	   SparsePixelType  type         = static_cast<SparsePixelType> (static_cast<int> (cellDecoder( hotPixelData )["sparsePixelType"]));
//		TrackerDataImpl * zsData = dynamic_cast< TrackerDataImpl * > ( zsInputCollectionVec->getElementAt( i ) );
//		SparsePixelType   type   = static_cast<SparsePixelType> ( static_cast<int> (cellDecoder( zsData )["sparsePixelType"]) );


	   int sensorID              = static_cast<int > ( cellDecoder( hotPixelData )["sensorID"] );
//           streamlog_out ( MESSAGE ) << "sensorID: " << sensorID << " type " << kEUTelAPIXSparsePixel << " ?= " << type << endl; 

           if( type  == kEUTelAPIXSparsePixel)
           {  
             auto_ptr<EUTelSparseDataImpl<EUTelAPIXSparsePixel > > apixData(new EUTelSparseDataImpl<EUTelAPIXSparsePixel> ( hotPixelData ));
             std::vector<EUTelAPIXSparsePixel*> apixPixelVec;
 	     //auto_ptr<EUTelAPIXSparsePixel> apixPixel( new EUTelAPIXSparsePixel );
 	     EUTelAPIXSparsePixel apixPixel;
	     //Push all single Pixels of one plane in the apixPixelVec

             for ( unsigned int iPixel = 0; iPixel < apixData->size(); iPixel++ ) 
             {
              std::vector<int> apixColVec();
              apixData->getSparsePixelAt( iPixel, &apixPixel);
//	       apixPixelVec.push_back(new EUTelAPIXSparsePixel(apixPixel));
//              streamlog_out ( MESSAGE ) << iPixel << " of " << apixData->size() << " HotPixelInfo:  " << apixPixel.getXCoord() << " " << apixPixel.getYCoord() << " " << apixPixel.getSignal() << " " << apixPixel.getChip() << " " << apixPixel.getTime()<< endl;
              try
              {
                 char ix[100];
                 sprintf(ix, "%d,%d,%d", sensorID, apixPixel.getXCoord(), apixPixel.getYCoord() ); 
                 _hotPixelMap[ix] = true;             
              }
              catch(...)
              {
                 std::cout << "can not add pixel " << std::endl;
//               std::cout << sensorID << " " << apixPixel.getXCoord() << " " << apixPixel.getYCoord() << " " << std::endl;   
              }
             }

           }  	
           else if( type  ==  kEUTelSimpleSparsePixel )
           {  
              auto_ptr<EUTelSparseClusterImpl< EUTelSimpleSparsePixel > > m26Data( new EUTelSparseClusterImpl< EUTelSimpleSparsePixel >   ( hotPixelData ) );

              std::vector<EUTelSimpleSparsePixel*> m26PixelVec;
	      EUTelSimpleSparsePixel m26Pixel;
  	      //Push all single Pixels of one plane in the m26PixelVec

             for ( unsigned int iPixel = 0; iPixel < m26Data->size(); iPixel++ ) 
             {
              std::vector<int> m26ColVec();
              m26Data->getSparsePixelAt( iPixel, &m26Pixel);
//	       apixPixelVec.push_back(new EUTelAPIXSparsePixel(apixPixel));
              streamlog_out ( MESSAGE ) << iPixel << " of " << m26Data->size() << " HotPixelInfo:  " << m26Pixel.getXCoord() << " " << m26Pixel.getYCoord() << " " << m26Pixel.getSignal() << endl;
              try
              {
                 char ix[100];
                 sprintf(ix, "%d,%d,%d", sensorID, m26Pixel.getXCoord(), m26Pixel.getYCoord() ); 
                 _hotPixelMap[ix] = true;             
              }
              catch(...)
              {
                 std::cout << "can not add pixel " << std::endl;
                 std::cout << sensorID << " " << m26Pixel.getXCoord() << " " << m26Pixel.getYCoord() << " " << std::endl;   
              }
             }
           }
       }

}
 
void EUTelMille::processEvent (LCEvent * event) {

  if ( isFirstEvent() )
  {
    FillHotPixelMap(event);

    if ( _applyToReferenceHitCollection ) 
    {
       _referenceHitVec = dynamic_cast < LCCollectionVec * > (event->getCollection( _referenceHitCollectionName));
    }
  }

  if (_iEvt % 100 == 0) 
  {
    streamlog_out( MESSAGE2 ) << "Processing event "
                              << setw(6) << setiosflags(ios::right) << event->getEventNumber() << " in run "
                              << setw(6) << setiosflags(ios::right) << setfill('0')  << event->getRunNumber() << setfill(' ')
                              << " (Total = " << setw(10) << _iEvt << ")" << resetiosflags(ios::left) << endl;
    streamlog_out( MESSAGE2 ) << "Currently having " << _nMilleDataPoints << " data points in "
                              << _nMilleTracks << " tracks " << endl;
  }

  if( _nMilleTracks > _maxTrackCandidatesTotal )
  {
      throw StopProcessingException(this);
  }
  
  // fill resolution arrays
  for (int help = 0; help < _nPlanes; help++) {
    _telescopeResolX[help] = _telescopeResolution;
    _telescopeResolY[help] = _telescopeResolution;
  }

  EUTelEventImpl * evt = static_cast<EUTelEventImpl*> (event) ;

  if ( evt->getEventType() == kEORE ) {
    streamlog_out ( DEBUG2 ) << "EORE found: nothing else to do." << endl;
    return;
  }

  std::vector<std::vector<EUTelMille::HitsInPlane> > _hitsArray(_nPlanes - _nExcludePlanes, std::vector<EUTelMille::HitsInPlane>());
  std::vector<int> indexconverter (_nPlanes,-1);
 
  std::vector<std::vector<EUTelMille::HitsInPlane> > _allHitsArray(_nPlanes, std::vector<EUTelMille::HitsInPlane>());
  

//  printf("887: nPlanes %5d , excludePlanes %2d  \n", _nPlanes, _nExcludePlanes );
//  if ( _nExcludePlanes > 0 )
  {
 
    
      int icounter = 0;
      for(int i = 0; i < _nPlanes; i++)
      {
          int excluded = 0; //0 - not excluded, 1 - excluded
          if ( _nExcludePlanes > 0 )
          {
              for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
                  if (i == _excludePlanes[helphelp] ) {
                      excluded = 1;
                      break;//leave the for loop
                  }
              }
          }
          if(excluded == 1)
              indexconverter[i] = -1;
          else
          {
              indexconverter[i] = icounter;
              icounter++;
          }
 
//        printf("907: excludePlanes %2d of %2d (%2d) \n", i, _nPlanes, indexconverter[i] );
      }
  }

//  std::vector<std::vector<EUTelMille::HitsInPlane> > _hitsArray(_nPlanes, std::vector<EUTelMille::HitsInPlane>());
/*
  printf(" inputMode = %5d  hitCollectionName.size = %5d, indexconverter.size= %5d \n", _inputMode, _hitCollectionName.size(), indexconverter.size() );
     for(size_t i =0;i < indexconverter.size();i++)
      {
          std::cout << " " << indexconverter[i] ;
      }
     std::cout << std::endl;
*/
  if (_inputMode != 1 && _inputMode != 3)
    for(size_t i =0;i < _hitCollectionName.size();i++)
      {

        LCCollection* collection;
        try {
          collection = event->getCollection(_hitCollectionName[i]);
        } catch (DataNotAvailableException& e) {
          streamlog_out ( WARNING2 ) << "No input collection " << _hitCollectionName[i] << " found for event " << event->getEventNumber()
                                     << " in run " << event->getRunNumber() << endl;
          throw SkipEventException(this);
        }
        int layerIndex = -1;
        HitsInPlane hitsInPlane;

        // check if running in input mode 0 or 2
        if (_inputMode == 0) {

          // loop over all hits in collection
          for ( int iHit = 0; iHit < collection->getNumberOfElements(); iHit++ ) {

            TrackerHitImpl * hit = static_cast<TrackerHitImpl*> ( collection->getElementAt(iHit) );
             
            if( hitContainsHotPixels(hit) )
            {
//              streamlog_out ( WARNING ) << "Hit " << i << " contains hot pixels; skip this one. " << endl;
              continue;
            }

            LCObjectVec clusterVector = hit->getRawHits();

            EUTelVirtualCluster * cluster;

            if ( hit->getType() == kEUTelBrickedClusterImpl ) {

               // fixed cluster implementation. Remember it
               //  can come from
               //  both RAW and ZS data
   
                cluster = new EUTelBrickedClusterImpl(static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
                
            } else if ( hit->getType() == kEUTelDFFClusterImpl ) {
              
              // fixed cluster implementation. Remember it can come from
              // both RAW and ZS data
              cluster = new EUTelDFFClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
            } else if ( hit->getType() == kEUTelFFClusterImpl ) {
              
              // fixed cluster implementation. Remember it can come from
              // both RAW and ZS data
              cluster = new EUTelFFClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
            } else if ( hit->getType() == kEUTelAPIXClusterImpl ) {
              
//              cluster = new EUTelSparseClusterImpl< EUTelAPIXSparsePixel >
//                 ( static_cast<TrackerDataImpl *> ( clusterVector[ 0 ]  ) );

                // streamlog_out(MESSAGE4) << "Type is kEUTelAPIXClusterImpl" << endl;
                TrackerDataImpl * clusterFrame = static_cast<TrackerDataImpl*> ( clusterVector[0] );
                // streamlog_out(MESSAGE4) << "Vector size is: " << clusterVector.size() << endl;

                cluster = new eutelescope::EUTelSparseClusterImpl< eutelescope::EUTelAPIXSparsePixel >(clusterFrame);
	      
	        // CellIDDecoder<TrackerDataImpl> cellDecoder(clusterFrame);
                eutelescope::EUTelSparseClusterImpl< eutelescope::EUTelAPIXSparsePixel > *apixCluster = new eutelescope::EUTelSparseClusterImpl< eutelescope::EUTelAPIXSparsePixel >(clusterFrame);
                
                int sensorID = apixCluster->getDetectorID();//static_cast<int> ( cellDecoder(clusterFrame)["sensorID"] );
         	    // cout << "Pixels at sensor " << sensorID << ": ";

                bool skipHit = false;
                for (int iPixel = 0; iPixel < apixCluster->size(); ++iPixel) 
                {
                    int pixelX, pixelY;
                    EUTelAPIXSparsePixel apixPixel;
                    apixCluster->getSparsePixelAt(iPixel, &apixPixel);
                    pixelX = apixPixel.getXCoord();
                    pixelY = apixPixel.getYCoord();
                    // cout << "(" << pixelX << "|" << pixelY << ") ";
		     
                    skipHit = !_rect.isInside(sensorID, pixelX, pixelY); 	      
                }
                // cout << endl;

                if (skipHit) 
                {
                    streamlog_out(MESSAGE4) << "Skipping cluster due to rectangular cut!" << endl;
                    continue;
                }
                if (skipHit) {
                    streamlog_out(MESSAGE4) << "TEST: This message should never appear!" << endl;                    
                }

            } else if ( hit->getType() == kEUTelSparseClusterImpl ) {


// streamlog_out(MESSAGE) << " found kEUTelSparseClusterImpl hit type " << endl;

              // ok the cluster is of sparse type, but we also need to know
              // the kind of pixel description used. This information is
              // stored in the corresponding original data collection.

              LCCollectionVec * sparseClusterCollectionVec = dynamic_cast < LCCollectionVec * > (evt->getCollection("original_zsdata"));
// streamlog_out(MESSAGE) << " sparseClusterCollectionVec = " << (sparseClusterCollectionVec) << endl;

             TrackerDataImpl * oneCluster = dynamic_cast<TrackerDataImpl*> (sparseClusterCollectionVec->getElementAt( 0 ));
              CellIDDecoder<TrackerDataImpl > anotherDecoder(sparseClusterCollectionVec);
              SparsePixelType pixelType = static_cast<SparsePixelType> ( static_cast<int> ( anotherDecoder( oneCluster )["sparsePixelType"] ));

              // now we know the pixel type. So we can properly create a new
              // instance of the sparse cluster
              if ( pixelType == kEUTelSimpleSparsePixel ) {

                cluster = new EUTelSparseClusterImpl< EUTelSimpleSparsePixel >
                  ( static_cast<TrackerDataImpl *> ( clusterVector[ 0 ]  ) );

              } else if ( pixelType == kEUTelAPIXSparsePixel ) {

                cluster = new EUTelSparseClusterImpl<EUTelAPIXSparsePixel >
                  ( static_cast<TrackerDataImpl *> ( clusterVector[ 0 ]  ) );

              } else {
                streamlog_out ( ERROR4 ) << "Unknown pixel type.  Sorry for quitting." << endl;
                throw UnknownDataTypeException("Pixel type unknown");
              }

 
            } else {
              throw UnknownDataTypeException("Unknown cluster type");
            }

            if ( 
                    hit->getType() == kEUTelDFFClusterImpl 
                    ||
                    hit->getType() == kEUTelFFClusterImpl 
                    ||
                    hit->getType() == kEUTelSparseClusterImpl 
                    ) 
            {
                if(cluster->getTotalCharge() <= getMimosa26ClusterChargeMin() )
                {
                    printf("(1) Thin cluster (charge <=1), hit type: %5d  detector: %5d\n", hit->getType(), cluster->getDetectorID() );
                    delete cluster; 
                    continue;
                }
            }
            else
            {
//                    printf("(e) Thin cluster (charge <=1), hit type: %5d  detector: %5d\n", hit->getType(), cluster->getDetectorID() );                
            }
 
//            double minDistance =  numeric_limits< double >::max() ;
            double * hitPosition = const_cast<double * > (hit->getPosition());

            unsigned int localSensorID   = guessSensorID( hitPosition );
            localSensorID   = guessSensorID( hit );

              
            layerIndex = _sensorIDVecMap[localSensorID] ;
//printf("sensor %5d at %5d with %5.3f %5.3f %5.3f  [cluster detID:%5d]\n", localSensorID, layerIndex, hitPosition[0], hitPosition[1], hitPosition[2], cluster->getDetectorID() );
/*
            for ( int i = 0 ; i < (int)_siPlaneZPosition.size(); i++ )
              {
                double distance = std::abs( hitPosition[2] - _siPlaneZPosition[i] );
                if ( distance < minDistance )
                  {
                    minDistance = distance;
                    layerIndex = i;
                  }
              }
*/
//            if ( minDistance > 30 /* mm */ ) {
              // advice the user that the guessing wasn't successful
//              streamlog_out( WARNING3 ) << "A hit was found " << minDistance << " mm far from the nearest plane\n"
//                "Please check the consistency of the data with the GEAR file" << endl;
//            }


            // Getting positions of the hits.
            // ------------------------------
            hitsInPlane.measuredX = 1000 * hit->getPosition()[0];
            hitsInPlane.measuredY = 1000 * hit->getPosition()[1];
            hitsInPlane.measuredZ = 1000 * hit->getPosition()[2];

//           printf("hit %5d of %5d , at %-8.1f %-8.1f %-8.1f, %5d %5d \n", iHit , collection->getNumberOfElements(), hitsInPlane.measuredX, hitsInPlane.measuredY, hitsInPlane.measuredZ, indexconverter[layerIndex], layerIndex );
            // adding a requirement to use only fat clusters for alignement


            if(indexconverter[layerIndex] != -1)
            {
//               _hitsArray[indexconverter[layerIndex]].push_back(hitsInPlane);
//               printf("  _hitsArray size %5d indexconverter[%2d] = %2d, %8.3f, %8.3f, %8.3f \n", _hitsArray.size(), layerIndex, indexconverter[layerIndex],
//                       hitsInPlane.measuredX,
//                       hitsInPlane.measuredY,
//                       hitsInPlane.measuredZ);
            }

            _allHitsArray[layerIndex].push_back(hitsInPlane);
//            printf("allHitsArray[%2d:%2d]: size %5d[%5d] indexconverter[%2d] = %2d, %8.3f, %8.3f, %8.3f \n", layerIndex, localSensorID, 
//                       _allHitsArray.size(), _allHitsArray[layerIndex].size(), layerIndex, indexconverter[layerIndex],
//                      hitsInPlane.measuredX,
//                      hitsInPlane.measuredY,
//                      hitsInPlane.measuredZ);


            delete cluster; // <--- destroying the cluster

//COMMENT 190510-2227            if(indexconverter[layerIndex] != -1) 
//COMMENT            _hitsArray[indexconverter[layerIndex]].push_back(hitsInPlane);
            
//            _hitsArray[layerIndex].push_back(hitsInPlane);

          } // end loop over all hits in collection

        } else if (_inputMode == 2) {

#if defined( USE_ROOT ) || defined(MARLIN_USE_ROOT)

          const float resolX = _testModeSensorResolution;
          const float resolY = _testModeSensorResolution;

          const float xhitpos = gRandom->Uniform(-3500.0,3500.0);
          const float yhitpos = gRandom->Uniform(-3500.0,3500.0);

          const float xslope = gRandom->Gaus(0.0,_testModeXTrackSlope);
          const float yslope = gRandom->Gaus(0.0,_testModeYTrackSlope);

          // loop over all planes
          for (int help = 0; help < _nPlanes; help++) {

            // The x and y positions are given by the sums of the measured
            // hit positions, the detector resolution, the shifts of the
            // planes and the effect due to the track slopes.
            hitsInPlane.measuredX = xhitpos + gRandom->Gaus(0.0,resolX) + _testModeSensorXShifts[help] + _testModeSensorZPositions[help] * tan(xslope) - _testModeSensorGamma[help] * yhitpos - _testModeSensorBeta[help] * _testModeSensorZPositions[0];
            hitsInPlane.measuredY = yhitpos + gRandom->Gaus(0.0,resolY) + _testModeSensorYShifts[help] + _testModeSensorZPositions[help] * tan(yslope) + _testModeSensorGamma[help] * xhitpos - _testModeSensorAlpha[help] * _testModeSensorZPositions[help];
            hitsInPlane.measuredZ = _testModeSensorZPositions[help];
            if(indexconverter[help] != -1) 
              _hitsArray[indexconverter[help]].push_back(hitsInPlane);

//  ADDITIONAL PRINTOUTS   
//            printf("plane:%3d hit:%3d %8.3f %8.3f %8.3f \n", help, indexconverter[help], hitsInPlane.measuredX,hitsInPlane.measuredY,hitsInPlane.measuredZ);
       _hitsArray[help].push_back(hitsInPlane);
            _telescopeResolX[help] = resolX;
            _telescopeResolY[help] = resolY;
          } // end loop over all planes

#else // USE_ROOT

          throw MissingLibraryException( this, "ROOT" );

#endif


        } // end if check running in input mode 0 or 2

      }
/*
  for(int j=0;j<_nPlanes;j++) 
  for(int i=0;i<_allHitsArray[j].size();i++)
    {
      double x0 = _allHitsArray[j][i].measuredX;
      double y0 = _allHitsArray[j][i].measuredY;
      double z0 = _allHitsArray[j][i].measuredZ;
      printf("control print hit[%5d][%5d].of.%5d  at %5.3f %5.3f %5.3f \n", j   , i, _allHitsArray[j].size(),  x0, y0, z0);
    }
*/

  std::vector<int> fitplane(_nPlanes, 0);

  for (int help = 0; help < _nPlanes; help++) {
    fitplane[help] = 1;
  }

  int _nTracks = 0;

  int _nGoodTracks = 0;

  // check if running in input mode 0 or 2 => perform simple track finding
  if (_inputMode == 0 || _inputMode == 2) {

    // Find track candidates using the distance cuts
    // ---------------------------------------------
    //
    // This is done separately for different numbers of planes.

    std::vector<std::vector<int> > indexarray;

//  ADDITIONAL PRINTOUTS   
//    printf("=========\n");
    findtracks2(0, indexarray, std::vector<int>(), _allHitsArray, 0, 0);
//    printf("==== indexarray size i = %5d %5d hitsArray.size() = %3d \n", indexarray.size(), indexconverter.size(), _allHitsArray.size() );
    for(size_t i = 0; i < indexarray.size(); i++)
      {
        for(size_t j = 0; j <  _nPlanes; j++)
          {
//  ADDITIONAL PRINTOUTS   
//             printf("indexarray size i = %5d, j = %5d (%2d)\n", i, j, indexconverter[j]);

             if( indexarray[i][j] >= 0 )
             {              
               _xPos[i][j] = _allHitsArray[j][indexarray[i][j]].measuredX;
               _yPos[i][j] = _allHitsArray[j][indexarray[i][j]].measuredY;
               _zPos[i][j] = _allHitsArray[j][indexarray[i][j]].measuredZ;
             }
             else
             {
               _xPos[i][j] = 0.;
               _yPos[i][j] = 0.;
               _zPos[i][j] = 0.;
             }  
//             printf("track[%2d] plane[%2d] at [%08.1f %08.1f %08.1f] \n", i, j, _xPos[i][j], _yPos[i][j], _zPos[i][j]);
          }
      }

//return;
/* 
    findtracks(indexarray, std::vector<int>(), _hitsArray, 0, 0);
 
    for(size_t i = 0; i < indexarray.size(); i++)
      {
        for(size_t j = 0; j <  indexconverter.size(); j++)
          {

//  ADDITIONAL PRINTOUTS   
//             printf("indexarray size i = %5d, j = %5d (%2d)\n", i, j, indexconverter[j]);
              
             if(indexconverter[j] == -1)
             {
                //if the plane j is excluded set a fake hit. This hit
                //will be ignored by the fitter.
                   _xPos[i][j] = 0.0;
                   _yPos[i][j] = 0.0;
                   _zPos[i][j] = 0.0;
             }
             else
             {
                //if the plane j is not excluded. note that j must be
                //converted using indexconverter into the index number
                //of _hitsArray, because _hitsArray does not contain
                //excluded planes.
                _xPos[i][j] = _hitsArray[indexconverter[j]][indexarray[i][indexconverter[j]]].measuredX;
                _yPos[i][j] = _hitsArray[indexconverter[j]][indexarray[i][indexconverter[j]]].measuredY;
                _zPos[i][j] = _hitsArray[indexconverter[j]][indexarray[i][indexconverter[j]]].measuredZ;
             }
          }
      }
*/

    _nTracks = (int) indexarray.size();
//  ADDITIONAL PRINTOUTS   
//   printf("indexarray size i = %5d  _nTracks = %5d \n", indexarray.size(), _nTracks );
 
    // end check if running in input mode 0 or 2 => perform simple track finding
  } else if (_inputMode == 1) {
    LCCollection* collection;
    try {
      collection = event->getCollection(_trackCollectionName);
    } catch (DataNotAvailableException& e) {
      streamlog_out ( WARNING2 ) << "No input track collection " << _trackCollectionName  << " found for event " << event->getEventNumber()
                                 << " in run " << event->getRunNumber() << endl;
      throw SkipEventException(this);
    }
    const int nTracksHere = collection->getNumberOfElements();

    streamlog_out ( MILLEMESSAGE ) << "Number of tracks available in track collection: " << nTracksHere << endl;

    // loop over all tracks
    for (int nTracksEvent = 0; nTracksEvent < nTracksHere && nTracksEvent < _maxTrackCandidates; nTracksEvent++) {

      Track *TrackHere = dynamic_cast<Track*> (collection->getElementAt(nTracksEvent));

      // hit list assigned to track

      std::vector<EVENT::TrackerHit*> TrackHitsHere = TrackHere->getTrackerHits();

      // check for a hit in every plane

      if (_nPlanes == int(TrackHitsHere.size() / 2)) {

        // assume hits are ordered in z! start counting from 0
        int nPlaneHere = 0;

        // loop over all hits and fill arrays
        for (int nHits = 0; nHits < int(TrackHitsHere.size()); nHits++) {

          TrackerHit *HitHere = TrackHitsHere.at(nHits);

          // hit positions
          const double *PositionsHere = HitHere->getPosition();

          // assume fitted hits have type 32
          if ( HitHere->getType() == 32 ) {

            // fill hits to arrays
            _xPos[nTracksEvent][nPlaneHere] = PositionsHere[0] * 1000;
            _yPos[nTracksEvent][nPlaneHere] = PositionsHere[1] * 1000;
            _zPos[nTracksEvent][nPlaneHere] = PositionsHere[2] * 1000;

            nPlaneHere++;

          } // end assume fitted hits have type 32

        } // end loop over all hits and fill arrays

        _nTracks++;

      } else {

        streamlog_out ( MILLEMESSAGE ) << "Dropping track " << nTracksEvent << " because there is not a hit in every plane assigned to it." << endl;

      }

    } // end loop over all tracks

  } else if (_inputMode == 3) {
    LCCollection* collection;
    try {
      collection = event->getCollection(_trackCollectionName);
    } catch (DataNotAvailableException& e) {
      streamlog_out ( WARNING2 ) << "No input track collection " << _trackCollectionName  << " found for event " << event->getEventNumber()
                                 << " in run " << event->getRunNumber() << endl;
      throw SkipEventException(this);
    }
    const int nTracksHere = collection->getNumberOfElements();
    
    // loop over all tracks
    for (int nTracksEvent = 0; nTracksEvent < nTracksHere && nTracksEvent < _maxTrackCandidates; nTracksEvent++) {

      Track *TrackHere = dynamic_cast<Track*> (collection->getElementAt(nTracksEvent));

      // hit list assigned to track
      std::vector<EVENT::TrackerHit*> TrackHitsHere = TrackHere->getTrackerHits();

      int number_of_planes = int((TrackHitsHere.size() - _excludePlanes.size() )/ 2);
//    int number_of_planes = int(TrackHitsHere.size() / 2);
      if ( _siPlanesParameters->getSiPlanesType() == _siPlanesParameters->TelescopeWithDUT ) {
        ++number_of_planes;
      }
      // check for a hit in every telescope plane. this needs probably
      // some further investigations. perhaps it fails if some planes
      // were excluded in the track fitter. but it should work
      if ((_nPlanes  - static_cast<int>(_excludePlanes.size()))== number_of_planes)
        {
          for(size_t i =0;i < _hitCollectionName.size();i++)
 //     // check for a hit in every telescope plane
 //     if (_siPlanesParameters->getSiPlanesNumber() == number_of_planes)
          {
              LCCollection* collection;
              try {
                collection = event->getCollection(_hitCollectionName[i]);
              } catch (DataNotAvailableException& e) {
                streamlog_out ( WARNING2 ) << "No input collection " << _hitCollectionName[i] << " found for event " << event->getEventNumber()
                                           << " in run " << event->getRunNumber() << endl;
                throw SkipEventException(this);
              }
              for ( int iHit = 0; iHit < collection->getNumberOfElements(); iHit++ )
                {
                  TrackerHitImpl *hit = static_cast<TrackerHitImpl*> ( collection->getElementAt(iHit) );

                  LCObjectVec clusterVector = hit->getRawHits();
                  EUTelVirtualCluster *cluster;

                  if ( hit->getType() == kEUTelBrickedClusterImpl ) {

                    // fixed cluster implementation. Remember it can come from
                    // both RAW and ZS data
                    cluster = new EUTelBrickedClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
                  }
                  else if ( hit->getType() == kEUTelDFFClusterImpl ) {

                    // fixed cluster implementation. Remember it can come from
                    // both RAW and ZS data
                    cluster = new EUTelDFFClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
                  }
                  else if ( hit->getType() == kEUTelFFClusterImpl ) {

                    // fixed cluster implementation. Remember it can come from
                    // both RAW and ZS data
                    cluster = new EUTelFFClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
                  }
                  else if ( hit->getType() == kEUTelSparseClusterImpl ) {
                    //copy and paste from the inputmode 0
                    // code. needs to be tested ...

                    // ok the cluster is of sparse type, but we also need to know
                    // the kind of pixel description used. This information is
                    // stored in the corresponding original data collection.

                    LCCollectionVec * sparseClusterCollectionVec = dynamic_cast < LCCollectionVec * > (evt->getCollection("original_zsdata"));
                    TrackerDataImpl * oneCluster = dynamic_cast<TrackerDataImpl*> (sparseClusterCollectionVec->getElementAt( 0 ));
                    CellIDDecoder<TrackerDataImpl > anotherDecoder(sparseClusterCollectionVec);
                    SparsePixelType pixelType = static_cast<SparsePixelType> ( static_cast<int> ( anotherDecoder( oneCluster )["sparsePixelType"] ));

                    // now we know the pixel type. So we can properly create a new
                    // instance of the sparse cluster
                    if ( pixelType == kEUTelSimpleSparsePixel ) {

                      cluster = new EUTelSparseClusterImpl< EUTelSimpleSparsePixel >
                        ( static_cast<TrackerDataImpl *> ( clusterVector[ 0 ]  ) );

                    } else {
                      streamlog_out ( ERROR4 ) << "Unknown pixel type.  Sorry for quitting." << endl;
                      throw UnknownDataTypeException("Pixel type unknown");
                    }

                  } else if ( hit->getType() == kEUTelAPIXClusterImpl ) {
              
                      cluster = new EUTelSparseClusterImpl< EUTelAPIXSparsePixel >
                          ( static_cast<TrackerDataImpl *> ( clusterVector[ 0 ]  ) );
 
                  } else {
                    throw UnknownDataTypeException("Unknown cluster type");
                  }

                  std::vector<EUTelMille::HitsInPlane> hitsplane;

                  if ( 
                    hit->getType() == kEUTelDFFClusterImpl 
                    ||
                    hit->getType() == kEUTelFFClusterImpl 
                    ||
                    hit->getType() == kEUTelSparseClusterImpl 
                    ) 
                  {
                      if(cluster->getTotalCharge() <= getMimosa26ClusterChargeMin() )
                      {
//                          printf("(2) Thin cluster (charge <=1), hit type: %5d  \n", hit->getType() );
                          delete cluster; 
                          continue;
                      }
                  }


                  hitsplane.push_back(
                          EUTelMille::HitsInPlane(
                              1000 * hit->getPosition()[0],
                              1000 * hit->getPosition()[1],
                              1000 * hit->getPosition()[2]
                              )
                          );
//                  printf("hit %5d pos:%8.3f %8.3f %8.3f \n", iHit,  hit->getPosition()[0],  hit->getPosition()[1], hit->getPosition()[2]  );
 
                  double measuredz = hit->getPosition()[2];

                  delete cluster; // <--- destroying the cluster
                  for (int nHits = 0; nHits < int(TrackHitsHere.size()); nHits++)  // end loop over all hits and fill arrays
                  {
                      TrackerHit *HitHere = TrackHitsHere.at(nHits);

                      // hit positions
                      const double *PositionsHere = HitHere->getPosition();
//                  printf("fithit %5d pos:%8.3f %8.3f %8.3f \n", nHits, HitHere->getPosition()[0], HitHere->getPosition()[1], HitHere->getPosition()[2]  );
 
                      //assume that fitted hits have type 32.
                      //the tracker hit will be excluded if the
                      //distance to the hit from the hit collection
                      //is larger than 5 mm. this requirement should reject
                      //reconstructed hits in the DUT in order to
                      //avoid double counting.

//  ADDITIONAL PRINTOUTS   
//                      printf("measZ = %8.3f posHere[2] = %8.3f  type = %5d \n", measuredz, PositionsHere[2], HitHere->getType() );
                      if( std::abs( measuredz - PositionsHere[2] ) > 5.0 /* mm */)
                        {
                          if ( HitHere->getType()  == 32 )
                            {
                              hitsplane.push_back(
                                      EUTelMille::HitsInPlane(
                                          PositionsHere[0] * 1000,
                                          PositionsHere[1] * 1000,
                                          PositionsHere[2] * 1000
                                          )
                                      );
                            } // end assume fitted hits have type 32
                        }
                    }
                  //sort the array such that the hits are ordered
                  //in z assuming that z is constant over all
                  //events for each plane
                  std::sort(hitsplane.begin(), hitsplane.end());
                          
          
                  //now the array is filled into the track
                  //candidates array
                  for(int i = 0; i < _nPlanes; i++)
                    {
                      _xPos[_nTracks][i] = hitsplane[i].measuredX;
                      _yPos[_nTracks][i] = hitsplane[i].measuredY;
                      _zPos[_nTracks][i] = hitsplane[i].measuredZ;
                    }
                  _nTracks++; //and we found an additional track candidate.
                }
            }
          //end of the loop
        } else {

        streamlog_out ( MILLEMESSAGE ) << "Dropping track " << nTracksEvent << " because there is not a hit in every plane assigned to it." << endl;
      }

    } // end loop over all tracks

  }

//  if (_nTracks == _maxTrackCandidates) {
//    streamlog_out ( WARNING2 ) << "Maximum number of track candidates reached. Maybe further tracks were skipped" << endl;
//  }

  streamlog_out ( MILLEMESSAGE ) << "Number of hits in the individual planes: ";
//   for(size_t i = 0; i < _hitsArray.size(); i++)
//    streamlog_out ( MILLEMESSAGE ) << _hitsArray[i].size() << " ";
  for(size_t i = 0; i < _allHitsArray.size(); i++)
    streamlog_out ( MILLEMESSAGE ) << _allHitsArray[i].size() << " ";
  streamlog_out ( MILLEMESSAGE ) << endl;

  streamlog_out ( MILLEMESSAGE ) << "Number of track candidates found: " << _iEvt << ": " << _nTracks << endl;

  // Perform fit for all found track candidates
  // ------------------------------------------

  // only one track or no single track event
  if (_nTracks == 1 || _onlySingleTrackEvents == 0) {

#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
    std::vector<double> lambda;
    double par_c0 = 0.0;
    double par_c1 = 0.0;
    lambda.reserve(_nPlanes);
//   lambda.reserve(_nPlanes-_nExcludePlanes);
    bool validminuittrack = false;
#endif

    double Chiquare[2] = {0,0};
    double angle[2] = {0,0};

    // loop over all track candidates
    for (int track = 0; track < _nTracks; track++) {
//printf("loop over tracks %5d \n", track);

      _xPosHere = new double[_nPlanes];
      _yPosHere = new double[_nPlanes];
      _zPosHere = new double[_nPlanes];

      for (int help = 0; help < _nPlanes; help++) {
        _xPosHere[help] = _xPos[track][help];
        _yPosHere[help] = _yPos[track][help];
        _zPosHere[help] = _zPos[track][help];
      }

      Chiquare[0] = 0.0;
      Chiquare[1] = 0.0;

      streamlog_out ( MILLEMESSAGE ) << "Adding track using the following coordinates: ";

      // loop over all planes
      for (int help = 0; help < _nPlanes; help++) 
      {
        int excluded = 0;

        // check if actual plane is excluded
        if (_nExcludePlanes > 0) {
          for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
            if (help == _excludePlanes[helphelp]) {
              excluded = 1;
            }
          }
        }

        if (excluded == 0) {
          streamlog_out ( MILLEMESSAGE ) << _xPosHere[help] << " " << _yPosHere[help] << " " << _zPosHere[help] << "   ";
        }

      } // end loop over all planes

      streamlog_out ( MILLEMESSAGE ) << endl;
      
      if(_alignMode == 3)
        {
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
          //use minuit to find tracks
          //hitsarray.clear();
          //hitsarray.reserve(_nPlanes-_nExcludePlanes);
          int    mean_n = 0  ;
          double mean_x = 0.0;
          double mean_y = 0.0;
          double mean_z = 0.0;
          int index_hitsarray=0;
          int index_firsthit=0;
          double x0 = -1.;
          double y0 = -1.;
          double z0 = -1.;
          for (int help = 0; help < _nPlanes; help++) 
          {
            bool excluded = false;
            // check if actual plane is excluded
            if (_nExcludePlanes > 0) 
            {
              for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) 
              {
                if (help == _excludePlanes[helphelp]) 
                {
                  excluded = true;
                }
              }
            }
            const double x = _xPos[track][help];
            const double y = _yPos[track][help];
            const double z = _zPos[track][help];
            if( abs(x)>1e-06 && abs(y)>1e-06 && abs(z)>1e-06 && index_firsthit == 0 )
            {
              index_firsthit = help; 
              x0 = _xPos[track][help];
              y0 = _yPos[track][help];
              z0 = _zPos[track][help];
            }
            const double xresid = x0 - x;
            const double yresid = y0 - y;
            const double zresid = z0 - z;
            if ( xresid < _residualsXMin[help] || xresid > _residualsXMax[help]) 
              {
                continue;
              }
            if ( yresid < _residualsYMin[help] || yresid > _residualsYMax[help]) 
              {
                continue;
              }
 
            if(!excluded)
              {
                double sigmax  = _resolutionX[help];
                double sigmay  = _resolutionY[help];
                double sigmaz  = _resolutionZ[help];
                  
                if( abs(x)>1e-06 && abs(y)>1e-06 && abs(z)>1e-06 )
                {
                  mean_z += z;
                  mean_x += x;
                  mean_y += y;
                  mean_n++;  
//                  printf(" track %5d plane %5d x:%5.2e y:%5.2e z:%5.2e total planes:%5d \n", track, help, x,y,z, mean_n);
                } 

                if( abs(x)<1e-06 && abs(y)<1e-06 && abs(z)<1e-06 )
                {
                  sigmax = 1000000.;
                  sigmay = 1000000.;
                  sigmaz = 1000000.;
                }

              //   hitsarray.push_back(hit(
//                                         x, y, z,
//                                         _resolutionX[help],_resolutionY[help], _resolutionZ[help],
//                                         help
//                                         ));

                hitsarray[index_hitsarray] = (hit(
                                                  x, y, z,
                                                  sigmax, sigmay, sigmaz,
                                                  help
                                                  ));
                
                index_hitsarray++;
              }
          }
//          mean_z = mean_z / (double)(number_of_datapoints);
//          mean_x = mean_x / (double)(number_of_datapoints);
//          mean_y = mean_y / (double)(number_of_datapoints);
          mean_z = mean_z / (double)(mean_n);
          mean_x = mean_x / (double)(mean_n);
          mean_y = mean_y / (double)(mean_n);

          if( _nPlanes - mean_n > getAllowedMissingHits() ) 
          {
             continue;
          }
          else
          {  
/*             for(size_t i = 0; i< mean_n; i++)
             {
               const double x = hitsarray[i].x;
               const double y = hitsarray[i].y;
               const double z = hitsarray[i].z;
               const int help = hitsarray[i].planenumber;
               printf(" track %5d plane %5d x:%5.2e y:%5.2e z:%5.2e total planes:%5d \n", track, help, x,y,z, mean_n);
             }
*/          }

          static bool firstminuitcall = true;
          
          if(firstminuitcall)
          {
              gSystem->Load("libMinuit");//is this really needed?
              firstminuitcall = false;
          }          
          TMinuit *gMinuit = new TMinuit(4);  //initialize TMinuit with a maximum of 4 params
          
          //  set print level (-1 = quiet, 0 = normal, 1 = verbose)
          gMinuit->SetPrintLevel(-1);
  
          gMinuit->SetFCN(fcn_wrapper);
          
          double arglist[10];
          int ierflg = 0;

   
          // minimization strategy (1 = standard, 2 = slower)
          arglist[0] = 2;
          gMinuit->mnexcm("SET STR",arglist,2,ierflg);
          
          // set error definition (1 = for chi square)
          arglist[0] = 1;
          gMinuit->mnexcm("SET ERR",arglist,1,ierflg);
          
          //analytic track fit to guess the starting parameters
          double sxx = 0.0;
          double syy = 0.0;
          double szz = 0.0;
            
          double szx = 0.0;
          double szy = 0.0;
            
          for(size_t i = 0; i< number_of_datapoints; i++)
          {
            const double x = hitsarray[i].x;
            const double y = hitsarray[i].y;
            const double z = hitsarray[i].z;
            if( abs(x)>1e-06 && abs(y)>1e-06 && abs(z)>1e-06 )
            {
              sxx += pow(x-mean_x,2);
              syy += pow(y-mean_y,2);
              szz += pow(z-mean_z,2);
            
              szx += (x-mean_x)*(z-mean_z);
              szy += (y-mean_y)*(z-mean_z);
            }
          }
          double linfit_x_a1 = szx/szz; //slope
          double linfit_y_a1 = szy/szz; //slope
            
          double linfit_x_a0 = mean_x - linfit_x_a1 * mean_z; //offset
          double linfit_y_a0 = mean_y - linfit_y_a1 * mean_z; //offset
            
          double del= -1.0*atan(linfit_y_a1);//guess of delta
          double ps = atan(linfit_x_a1/sqrt(1.0+linfit_y_a1*linfit_y_a1));//guess
                                                                          //of psi
            
          //  Set starting values and step sizes for parameters
          Double_t vstart[4] = {linfit_x_a0, linfit_y_a0, del, ps};
          //duble vstart[4] = {0.0, 0.0, 0.0, 0.0};
          double step[4] = {0.01, 0.01, 0.01, 0.01};
            
          gMinuit->mnparm(0, "b0", vstart[0], step[0], 0,0,ierflg);
          gMinuit->mnparm(1, "b1", vstart[1], step[1], 0,0,ierflg);
          gMinuit->mnparm(2, "delta", vstart[2], step[2], -1.0*TMath::Pi(), 1.0*TMath::Pi(),ierflg);
          gMinuit->mnparm(3, "psi", vstart[3], step[3], -1.0*TMath::Pi(), 1.0*TMath::Pi(),ierflg);
            
          //   gMinuit->FixParameter(2);
          //             gMinuit->FixParameter(3);
            

          //  Now ready for minimization step
          arglist[0] = 2000;
          arglist[1] = 0.01;
          gMinuit->mnexcm("MIGRAD", arglist ,1,ierflg);
            
          //gMinuit->Release(2);
          //gMinuit->Release(3);
            
          ////  Now ready for minimization step
          //arglist[0] = 8000;
          //arglist[1] = 1.0;
          //gMinuit->mnexcm("MIGRAD", arglist ,1,ierflg);
            
          bool ok = true;
            
          if(ierflg != 0)
          {
            ok = false;            
          }
                    
      //     //    calculate errors using MINOS. do we need this?
//           arglist[0] = 2000;
//           arglist[1] = 0.1;
//           gMinuit->mnexcm("MINOS",arglist,1,ierflg);
          
          //   get results from migrad
          double b0 = 0.0;
          double b1 = 0.0;
          double delta = 0.0;
          double psi = 0.0;
          double b0_error = 0.0;
          double b1_error = 0.0;
          double delta_error = 0.0;
          double psi_error = 0.0;
          
          gMinuit->GetParameter(0,b0,b0_error);
          gMinuit->GetParameter(1,b1,b1_error);
          gMinuit->GetParameter(2,delta,delta_error);
          gMinuit->GetParameter(3,psi,psi_error);
          
          double c0 = 1.0;
          double c1 = 1.0;
          double c2 = 1.0;
          if(ok)
            {
              c0 = TMath::Sin(psi);
              c1 = -1.0*TMath::Cos(psi) * TMath::Sin(delta);
              c2 = TMath::Cos(delta) * TMath::Cos(psi);
              par_c0 = c0;
              par_c1 = c1;
              validminuittrack = true;
              
              for (int help =0; help < _nPlanes; help++)
                {
                  const double x = _xPos[track][help];
                  const double y = _yPos[track][help];
                  const double z = _zPos[track][help];
                 
                  //calculate the lambda parameter
                  const double la = -1.0*b0*c0-b1*c1+c0*x+c1*y+sqrt(1-c0*c0-c1*c1)*z;
                  lambda.push_back(la);

		  if (_referenceHitVec == 0){                  
                  //determine the residuals without reference vector
                  _waferResidX[help] = b0 + la*c0 - x;
                  _waferResidY[help] = b1 + la*c1 - y;
                  _waferResidZ[help] = la*sqrt(1.0 - c0*c0 - c1*c1) - z;
//printf("MINUIT PRE sensor: %5d  %10.3f %10.3f %10.3f   %10.3f %10.3f %10.3f \n", help, x,y,z,  _waferResidX[help]+x, _waferResidY[help]+y, _waferResidZ[help]+z );

		  } else {
		    // use reference vector
		    TVector3 vpoint(b0,b1,0.);
		    TVector3 vvector(c0,c1,c2);
		    TVector3 point=Line2Plane(help, vpoint,vvector);

		    //determine the residuals
		    if( abs(x)>1e-06 && abs(y)>1e-06 && abs(z)>1e-06 )
		      {
			_waferResidX[help] = point[0] - x;
			_waferResidY[help] = point[1] - y;
			_waferResidZ[help] = point[2] - z;
		      }else{
		      _waferResidX[help] = 0.;
		      _waferResidY[help] = 0.;
		      _waferResidZ[help] = 0.;
		    }
		  }
		  //                 printf("FIT:   AFT sensor: %5d  [%10.3f %10.3f %10.3f]   [%10.3f %10.3f %10.3f] planes %5d \n", help, x,y,z,  _waferResidX[help], _waferResidY[help], _waferResidZ[help], mean_n );


/*
// try to recover planes which were exluded from the fit:
                  double mindistance=999999.;
                  TVector3 minpoint(0.,0.,0.);
                  TVector3 testpoint(0.,0.,0.);
//                  if( abs(x) < 1e-06 && abs(y) < 1e-06 && abs(x) < 1e-06 )
                  {
//                    printf("plane %2d with zero hit %5.3f %5.3f %5.3f \n", help, x,y,z);
                    for(int i=0;i<_allHitsArray[help].size(); i++)
                    {
                      double x0 = _allHitsArray[help][i].measuredX;
                      double y0 = _allHitsArray[help][i].measuredY;
                      double z0 = _allHitsArray[help][i].measuredZ;
                      testpoint = TVector3(x0,y0,z0);
                      TVector3 distance3d = testpoint-point;
                      double distance = distance3d.Mag(); 
                      if( mindistance > distance )
                      {
                        mindistance = distance;
                        minpoint = testpoint;
                      }
                      printf("could be replaced by  hit[%5d][%5d].of.%5d  at [%5.3f %5.3f %13.3f] [%5.3f %5.3f %13.3f]\n", 
                              help, i, _allHitsArray[help].size(),  x0, y0, z0, x,y,z);
                    }
                    if(mindistance <999999.)
                    {
                      printf("to be replaced by [%5.3f %5.3f %5.3f ][%5.3f %5.3f %5.3f] \n", point[0],point[1],point[2],minpoint[0],minpoint[1],minpoint[2] );
                    } 
                  } 
 */
                }
            }
          delete gMinuit;
#endif
        }
      else
        {
          // Calculate residuals
          FitTrack(int(_nPlanes),
                   _xPosHere,
                   _yPosHere,
                   _zPosHere,
                   _telescopeResolX,
                   _telescopeResolY,
                   Chiquare,
                   _waferResidX,
                   _waferResidY,
                   angle);
        }
      streamlog_out ( MILLEMESSAGE ) << "Residuals X: ";

      for (int help = 0; help < _nPlanes; help++) {
        streamlog_out ( MILLEMESSAGE ) << _waferResidX[help] << " ";
      }

      streamlog_out ( MILLEMESSAGE ) << endl;

      streamlog_out ( MILLEMESSAGE ) << "Residuals Y: ";

      for (int help = 0; help < _nPlanes; help++) {
        streamlog_out ( MILLEMESSAGE ) << _waferResidY[help] << " ";
      }

      streamlog_out ( MILLEMESSAGE ) << endl;

      streamlog_out ( MILLEMESSAGE ) << "Residuals Z: ";

      for (int help = 0; help < _nPlanes; help++) {
        streamlog_out ( MILLEMESSAGE ) << _waferResidZ[help] << " ";
      }

      streamlog_out ( MILLEMESSAGE ) << endl;


      int residualsXOkay = 1;
      int residualsYOkay = 1;

      // check if residal cuts are used
      if (_useResidualCuts != 0) 
      {

        // loop over all sensors
        for (int help = 0; help < _nPlanes; help++) 
        {
          int excluded = 0; //0 not excluded, 1 excluded
          if (_nExcludePlanes > 0) 
          {
            for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) 
            {
              if (help == _excludePlanes[helphelp]) 
              {
                excluded = 1;
              }
            }
          }
          if(excluded == 0)
          {
              if (_waferResidX[help] < _residualsXMin[help] || _waferResidX[help] > _residualsXMax[help]) 
              {
                residualsXOkay = 0;
              }
              if (_waferResidY[help] < _residualsYMin[help] || _waferResidY[help] > _residualsYMax[help]) 
              {
                residualsYOkay = 0;
              }
          }
// 
//          if (_waferResidX[help] < _residualsXMin[help] || _waferResidX[help] > _residualsXMax[help]) {
//            residualsXOkay = 0;
//          }
//          if (_waferResidY[help] < _residualsYMin[help] || _waferResidY[help] > _residualsYMax[help]) {
//            residualsYOkay = 0;
//          }

        } // end loop over all sensors

      } // end check if residual cuts are used

      if (_useResidualCuts != 0 && (residualsXOkay == 0 || residualsYOkay == 0)) {
        streamlog_out ( MILLEMESSAGE ) << "Track did not pass the residual cuts." << endl;
      }

      // apply track cuts (at the moment only residuals)
      if (_useResidualCuts == 0 || (residualsXOkay == 1 && residualsYOkay == 1)) {

        // Add track to Millepede
        // ---------------------------

        // Easy case: consider only shifts
        if (_alignMode == 2) {

          const int nLC = 4; // number of local parameters
          const int nGL = (_nPlanes - _nExcludePlanes) * 2; // number of global parameters

          float sigma = _telescopeResolution;

          float *derLC = new float[nLC]; // array of derivatives for local parameters
          float *derGL = new float[nGL]; // array of derivatives for global parameters

          int *label = new int[nGL]; // array of labels

          float residual;

          // create labels
          for (int help = 0; help < nGL; help++) {
            label[help] = help + 1;
          }

          for (int help = 0; help < nGL; help++) {
            derGL[help] = 0;
          }

          for (int help = 0; help < nLC; help++) {
            derLC[help] = 0;
          }

          int nExcluded = 0;

          // loop over all planes
          for (int help = 0; help < _nPlanes; help++) {

            int excluded = 0;

            // check if actual plane is excluded
            if (_nExcludePlanes > 0) {
              for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
                if (help == _excludePlanes[helphelp]) {
                  excluded = 1;
                  nExcluded++;
                }
              }
            }

            // if plane is not excluded
            if (excluded == 0) {

              int helphelp = help - nExcluded; // index of plane after
                                               // excluded planes have
                                               // been removed

              derGL[((helphelp * 2) + 0)] = -1;
              derLC[0] = 1;
              derLC[2] = _zPosHere[help];
              residual = _waferResidX[help];
              sigma    = _resolutionX[help];
              _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);

              derGL[((helphelp * 2) + 0)] = 0;
              derLC[0] = 0;
              derLC[2] = 0;

              derGL[((helphelp * 2) + 1)] = -1;
              derLC[1] = 1;
              derLC[3] = _zPosHere[help];
              residual = _waferResidY[help];
              sigma    = _resolutionY[help];
              _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);

              derGL[((helphelp * 2) + 1)] = 0;
              derLC[1] = 0;
              derLC[3] = 0;

              _nMilleDataPoints++;

            } // end if plane is not excluded

          } // end loop over all planes

          // clean up

          delete [] derLC;
          delete [] derGL;
          delete [] label;

          // Slightly more complicated: add rotation around the z axis
        } else if (_alignMode == 1) {

          const int nLC = 4; // number of local parameters
          const int nGL = _nPlanes * 3; // number of global parameters

          float sigma = _telescopeResolution;

          float *derLC = new float[nLC]; // array of derivatives for local parameters
          float *derGL = new float[nGL]; // array of derivatives for global parameters

          int *label = new int[nGL]; // array of labels

          float residual;

          // create labels
          for (int help = 0; help < nGL; help++) {
            label[help] = help + 1;
          }

          for (int help = 0; help < nGL; help++) {
            derGL[help] = 0;
          }

          for (int help = 0; help < nLC; help++) {
            derLC[help] = 0;
          }

          int nExcluded = 0;

          // loop over all planes
          for (int help = 0; help < _nPlanes; help++) {

            int excluded = 0;

            // check if actual plane is excluded
            if (_nExcludePlanes > 0) {
              for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
                if (help == _excludePlanes[helphelp]) {
                  excluded = 1;
                  nExcluded++;
                }
              }
            }

            // if plane is not excluded
            if (excluded == 0) {

              int helphelp = help - nExcluded; // index of plane after
                                               // excluded planes have
                                               // been removed

              derGL[((helphelp * 3) + 0)] = -1;
              derGL[((helphelp * 3) + 2)] = _yPosHere[help];
              derLC[0] = 1;
              derLC[2] = _zPosHere[help];
              residual = _waferResidX[help];
              sigma    = _resolutionX[help];
//              printf("sigma = %9.3f \n",sigma);
              _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);

              derGL[((helphelp * 3) + 0)] = 0;
              derGL[((helphelp * 3) + 2)] = 0;
              derLC[0] = 0;
              derLC[2] = 0;

              derGL[((helphelp * 3) + 1)] = -1;
              derGL[((helphelp * 3) + 2)] = -1 * _xPosHere[help];
              derLC[1] = 1;
              derLC[3] = _zPosHere[help];
              residual = _waferResidY[help];
              sigma    = _resolutionY[help];
              _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);

              derGL[((helphelp * 3) + 1)] = 0;
              derGL[((helphelp * 3) + 2)] = 0;
              derLC[1] = 0;
              derLC[3] = 0;

              _nMilleDataPoints++;

            } // end if plane is not excluded

          } // end loop over all planes

          // clean up

          delete [] derLC;
          delete [] derGL;
          delete [] label;

        } else if (_alignMode == 3) {
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
          if(validminuittrack)
            {
              const int nLC = 4; // number of local parameters
              const int nGL = _nPlanes * 6; // number of global parameters

              float sigma = _telescopeResolution;

              float *derLC = new float[nLC]; // array of derivatives for local parameters
              float *derGL = new float[nGL]; // array of derivatives for global parameters

              int *label = new int[nGL]; // array of labels

              float residual;

              // create labels
              for (int help = 0; help < nGL; help++) 
              {
                label[help] = help + 1;
              }

              for (int help = 0; help < nGL; help++) 
              {
                derGL[help] = 0;
              }

              for (int help = 0; help < nLC; help++) 
              {
                derLC[help] = 0;
              }

              int nExcluded = 0;

              // loop over all planes
             
              for (int help = 0; help < _nPlanes; help++) 
              {

                int excluded = 0;
/*
                // check if actual plane is excluded
                if (_nExcludePlanes > 0) {
                  for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {
                    if (help == _excludePlanes[helphelp]) {
                      excluded = 1;
                      nExcluded++;
                    }
                  }
                }
*/
                double sigmax = _resolutionX[help];
                double sigmay = _resolutionY[help];
                double sigmaz = _resolutionZ[help];

                if( 
                    abs(_xPosHere[help]) < 1e-06 &&
                    abs(_yPosHere[help]) < 1e-06 &&
                    abs(_zPosHere[help]) < 1e-06   
                   )
                {
                   sigmax *= 1000000.;
                   sigmay *= 1000000.;
                   sigmaz *= 1000000.;
                   continue;
                }

                // if plane is not excluded
//                 if (excluded == 0) {
//                   cout << "--" << endl;
//                   int helphelp = help - nExcluded; // index of plane after
//                   // excluded planes have
//                   // been removed

//                   //local parameters: b0, b1, c0, c1

//                   const double la = lambda[help];
              
//                   derGL[((helphelp * 6) + 0)] = 1.0;
//                   derGL[((helphelp * 6) + 1)] = 0.0;
//                   derGL[((helphelp * 6) + 2)] = 0.0;
//                   derGL[((helphelp * 6) + 3)] = 0.0;
//                   derGL[((helphelp * 6) + 4)] = -1.0*_zPosHere[help];
//                   derGL[((helphelp * 6) + 5)] = _yPosHere[help];

//                   derLC[0] = 1.0;
//                   derLC[1] = 0.0;
//                   derLC[2] = la;
//                   derLC[3] = 0.0;
            
//                   residual = _waferResidX[help];
//                   sigma = _resolutionX[help];
//                   for(int i =0; i<4;i++)
//                     cout << "a " << derLC[i] << endl;
                 
//                   _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);

             
//                   derGL[((helphelp * 6) + 0)] = 0.0;
//                   derGL[((helphelp * 6) + 1)] = 1.0;
//                   derGL[((helphelp * 6) + 2)] = 0.0;
//                   derGL[((helphelp * 6) + 3)] = _zPosHere[help];
//                   derGL[((helphelp * 6) + 4)] = _zPosHere[help];
//                   derGL[((helphelp * 6) + 5)] = -1.0*_xPosHere[help];

//                   derLC[0] = 0.0;
//                   derLC[1] = 1.0;
//                   derLC[2] = 0.0;
//                   derLC[3] = 1.0;
            
//                   residual = _waferResidY[help];
//                   sigma = _resolutionY[help];
//                   for(int i =0; i<4;i++)
//                     cout << "b " << derLC[i] << endl;
                  
//                   _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);
              
              
//                   derGL[((helphelp * 6) + 0)] = 0.0;
//                   derGL[((helphelp * 6) + 1)] = 0.0;
//                   derGL[((helphelp * 6) + 2)] = 1.0;
//                   derGL[((helphelp * 6) + 3)] = -1.0*_yPosHere[help];
//                   derGL[((helphelp * 6) + 4)] = _xPosHere[help];
//                   derGL[((helphelp * 6) + 5)] = 0.0;

//                   derLC[0] = 0.0;
//                   derLC[1] = 0.0;
//                   derLC[2] = -1.0*la*par_c0/sqrt(1.0-par_c0*par_c0-par_c1*par_c1);
//                   derLC[3] = -1.0*la*par_c1/sqrt(1.0-par_c0*par_c0-par_c1*par_c1);
            
//                   residual = _waferResidZ[help];
//                   sigma = _resolutionZ[help];
//                   for(int i =0; i<4;i++)
//                     cout << "c " << derLC[i] << endl;
                 
//                   _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigma);


//                   _nMilleDataPoints++;

//                 } // end if plane is not excluded

                if (excluded == 0) {
                  //     cout << "--" << endl;
                  int helphelp = help - nExcluded; // index of plane after
                  // excluded planes have
                  // been removed

                  //local parameters: b0, b1, c0, c1
                  const double la = lambda[help];

//                  double z_sensor = _siPlanesLayerLayout -> getSensitivePositionZ(help) + 0.5 * _siPlanesLayerLayout->getSensitiveThickness( help );
//                  z_sensor *= 1000;		// in microns
						// reset all derivatives to zero!
		  for (int i = 0; i < nGL; i++ ) 
                  {
		    derGL[i] = 0.000;
		  }

		  for (int i = 0; i < nLC; i++ ) 
                  {
		    derLC[i] = 0.000;
		  }

                  double x_sensor = 0.;
                  double y_sensor = 0.;
                  double z_sensor = 0.;

		  if (_referenceHitVec != 0){
		    for(int ii = 0 ; ii <  _referenceHitVec->getNumberOfElements(); ii++)
		      {
			EUTelReferenceHit* refhit = static_cast< EUTelReferenceHit*> ( _referenceHitVec->getElementAt(ii) ) ;
			if( _sensorIDVec[help] == refhit->getSensorID() )
			  {
			    //                      printf("found refhit [%2d] %5.2f %5.2f %5.2f for hit [%2d] %5.2f %5.2f %5.2f \n", 
			    //                              refhit->getSensorID(), refhit->getXOffset(), refhit->getYOffset(), refhit->getZOffset(),
			    //                              _sensorIDVec[help], _xPosHere[help], _yPosHere[help], _zPosHere[help] );
			    x_sensor =  refhit->getXOffset();
			    y_sensor =  refhit->getYOffset();
			    z_sensor =  refhit->getZOffset();
			  } 
		      }
		  }
                  x_sensor *= 1000.;
                  y_sensor *= 1000.;
                  z_sensor *= 1000.;




// track model : fit-reco => 
//   (a_X*x+b_X, a_Y*y+b_Y)  :   /  1   -g    b \   / x          \   :: shouldn't it be x-xcenter-of-the-sensor ??
//                           : - |  g    1   -a |   | y          |   ::  and y-ycenter-of-the-sensor ??   
//                           :   \ -b    a    1 /   \ z-z_sensor /   ::  == z-zcenter-of-the-sensor  ?? (already)
// make angles sings consistent with X->Y->Z->X rotations.
// correct likewise all matrices in ApplyAlignment processor
// Igor Rubinsky 09-10-2011
//
                  // shift in X
                  derGL[((helphelp * 6) + 0)] = -1.0;                                 // dx
                  derGL[((helphelp * 6) + 1)] =  0.0;                                 // dy
                  derGL[((helphelp * 6) + 2)] =  0.0;                                 // dz
                  // rotation in ZY ( alignment->getAlpfa() )
                  derGL[((helphelp * 6) + 3)] =      0.0;                             // alfa  - ZY :: Y->Z
                  derGL[((helphelp * 6) + 4)] = -1.0*(_zPosHere[help] - z_sensor);    // beta  - ZX :: Z->X
                  derGL[((helphelp * 6) + 5)] =  1.0*(_yPosHere[help] - y_sensor);                 // gamma - XY :: X->Y

                  derLC[0] = 1.0;
                  derLC[1] = 0.0;
                  derLC[2] = _zPosHere[help] + _waferResidZ[help];
                  derLC[3] = 0.0;
            
                  residual = _waferResidX[help];
//                  sigma = _resolutionX[help];
               //    for(int i =0; i<4;i++)
//                     cout << "a " << derLC[i] << endl;
                 
                  _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigmax);
//printf("helphelp %5d %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f \n", helphelp, _waferResidX[help], sigmax, _waferResidY[help], sigmay, _waferResidZ[help], sigmaz );

             
                  // shift in Y
                  derGL[((helphelp * 6) + 0)] =  0.0;
                  derGL[((helphelp * 6) + 1)] = -1.0; 
                  derGL[((helphelp * 6) + 2)] =  0.0;
                  // rotation in ZX
                  derGL[((helphelp * 6) + 3)] =  1.0*(_zPosHere[help] - z_sensor); 
                  derGL[((helphelp * 6) + 4)] =      0.0            ; 
                  derGL[((helphelp * 6) + 5)] = -1.0*(_xPosHere[help] - x_sensor);

                  derLC[0] = 0.0;
                  derLC[1] = 1.0;
                  derLC[2] = 0.0;
                  derLC[3] = _zPosHere[help] + _waferResidZ[help];
            
                  residual = _waferResidY[help];
//                  sigma = _resolutionY[help];
               //    for(int i =0; i<4;i++)
//                     cout << "b " << derLC[i] << endl;
                  
                  _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigmay);
              
              
                  // shift in Z
                  derGL[((helphelp * 6) + 0)] =  0.0;
                  derGL[((helphelp * 6) + 1)] =  0.0;
                  derGL[((helphelp * 6) + 2)] = -1.0; 
                  // rotation in XY
                  derGL[((helphelp * 6) + 3)] = -1.0*(_yPosHere[help]-y_sensor); 
                  derGL[((helphelp * 6) + 4)] =  1.0*(_xPosHere[help]-x_sensor);
                  derGL[((helphelp * 6) + 5)] =  0.0;

                  derLC[0] = 0.0;
                  derLC[1] = 0.0;
                  derLC[2] = _xPosHere[help] + _waferResidX[help];
                  derLC[3] = _yPosHere[help] + _waferResidY[help];
            
                  residual = _waferResidZ[help];
//                  sigma = _resolutionZ[help];
               //    for(int i =0; i<4;i++)
//                     cout << "c " << derLC[i] << endl;
                 
                  _mille->mille(nLC,derLC,nGL,derGL,label,residual,sigmaz);

//                  printf("residuals[%2d]: %7.2f %7.2f %7.2f :: x:%7.2f y:%7.2f \n", _sensorIDVec[help], _waferResidX[help], _waferResidY[help], _waferResidZ[help],
//                                                       (_xPosHere[help]-x_sensor),             
//                                                       (_yPosHere[help]-y_sensor)             
//                                                                                  );
 

                  _nMilleDataPoints++;

                } // end if plane is not excluded

              } // end loop over all planes

              // clean up

//              printf("finished track %5d \n", _nMilleDataPoints);

              delete [] derLC;
              delete [] derGL;
              delete [] label;
//              printf("finished track %5d DELETE OK\n", _nMilleDataPoints);

            }
#endif
        } else {

          streamlog_out ( ERROR2 ) << _alignMode << " is not a valid mode. Please choose 1,2 or 3." << endl;

        }

        _nGoodTracks++;

        // end local fit
        _mille->end();
//        printf("mille->end() OK \n", _nMilleTracks);

        _nMilleTracks++;

        // Fill histograms for individual tracks
        // -------------------------------------

#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)

        string tempHistoName;

        if ( _histogramSwitch ) {
          if ( AIDA::IHistogram1D* chi2x_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[_chi2XLocalname]) )
            chi2x_histo->fill(Chiquare[0]);
          else {
            streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _chi2XLocalname << endl;
            streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
            _histogramSwitch = false;
          }
        }

        if ( _histogramSwitch ) {
          if ( AIDA::IHistogram1D* chi2y_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[_chi2YLocalname]) )
            chi2y_histo->fill(Chiquare[1]);
          else {
            streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _chi2YLocalname << endl;
            streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
            _histogramSwitch = false;
          }
        }

//printf("coming to loope over detector plane sfor histos \n");

        // loop over all detector planes
        for( int iDetector = 0; iDetector < _nPlanes; iDetector++ ) {

          int sensorID = _orderedSensorID.at( iDetector );

          if ( 
              abs(_waferResidX[iDetector]) < 1e-06 &&  
              abs(_waferResidY[iDetector]) < 1e-06 &&  
              abs(_waferResidZ[iDetector]) < 1e-06 
             )   continue;


          if ( _histogramSwitch ) {
            tempHistoName = _residualXLocalname + "_d" + to_string( sensorID );
            if ( AIDA::IHistogram1D* residx_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[tempHistoName.c_str()]) )
            {
                residx_histo->fill(_waferResidX[iDetector]);

                tempHistoName = _residualXvsYLocalname + "_d" + to_string( sensorID );
                AIDA::IProfile1D* residxvsY_histo = dynamic_cast<AIDA::IProfile1D*>(_aidaHistoMapProf1D[tempHistoName.c_str()]) ;
                residxvsY_histo->fill(_yPosHere[iDetector], _waferResidX[iDetector]);
 
                tempHistoName = _residualXvsXLocalname + "_d" + to_string( sensorID );
                AIDA::IProfile1D* residxvsX_histo = dynamic_cast<AIDA::IProfile1D*>(_aidaHistoMapProf1D[tempHistoName.c_str()]) ;
                residxvsX_histo->fill(_xPosHere[iDetector], _waferResidX[iDetector]);
            }
            else
            {
              streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _residualXLocalname << endl;
              streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
              _histogramSwitch = false;
            }
          }

          if ( _histogramSwitch ) {
            tempHistoName = _residualYLocalname + "_d" + to_string( sensorID );
            if ( AIDA::IHistogram1D* residy_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[tempHistoName.c_str()]) )
            {
              residy_histo->fill(_waferResidY[iDetector]);

              tempHistoName = _residualYvsYLocalname + "_d" + to_string( sensorID );
              AIDA::IProfile1D* residyvsY_histo = dynamic_cast<AIDA::IProfile1D*>(_aidaHistoMapProf1D[tempHistoName.c_str()]) ;
              residyvsY_histo->fill(_yPosHere[iDetector], _waferResidY[iDetector]);
 
              tempHistoName = _residualYvsXLocalname + "_d" + to_string( sensorID );
              AIDA::IProfile1D* residyvsX_histo = dynamic_cast<AIDA::IProfile1D*>(_aidaHistoMapProf1D[tempHistoName.c_str()]) ;
              residyvsX_histo->fill(_xPosHere[iDetector], _waferResidY[iDetector]);
            }
            else
            {
              streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _residualYLocalname << endl;
              streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
              _histogramSwitch = false;
            }
          }
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
          if ( _histogramSwitch ) {
            tempHistoName = _residualZLocalname + "_d" + to_string( sensorID );
            if ( AIDA::IHistogram1D* residz_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[tempHistoName.c_str()]) )
            {
              residz_histo->fill(_waferResidZ[iDetector]);

              tempHistoName = _residualZvsYLocalname + "_d" + to_string( sensorID );
              AIDA::IProfile1D* residzvsY_histo = dynamic_cast<AIDA::IProfile1D*>(_aidaHistoMapProf1D[tempHistoName.c_str()]) ;
              residzvsY_histo->fill(_yPosHere[iDetector], _waferResidZ[iDetector]);
 
              tempHistoName = _residualZvsXLocalname + "_d" + to_string( sensorID );
              AIDA::IProfile1D* residzvsX_histo = dynamic_cast<AIDA::IProfile1D*>(_aidaHistoMapProf1D[tempHistoName.c_str()]) ;
              residzvsX_histo->fill(_xPosHere[iDetector], _waferResidZ[iDetector]);
 
//              residzvsY_histo->fill(_yPosHere[iDetector], _waferResidZ[iDetector]);
//              residzvsX_histo->fill(_xPosHere[iDetector], _waferResidZ[iDetector]);
            }
            else 
            {
              streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _residualZLocalname << endl;
              streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
              _histogramSwitch = false;
            }
          }

#endif

        } // end loop over all detector planes
//printf("coming to loope over detector plane sfor histos DONE\n");


#endif

      } // end if apply track cuts

      // clean up
      delete [] _zPosHere;
      delete [] _yPosHere;
      delete [] _xPosHere;

//printf("delete [] PosHere OK \n");


    } // end loop over all track candidates

  } // end if only one track or no single track event

  streamlog_out ( MILLEMESSAGE ) << "Finished fitting tracks in event " << _iEvt << endl;

#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)

  string tempHistoName;

  if ( _histogramSwitch ) {
    {
      stringstream ss;
      ss << _numberTracksLocalname << endl;
    }
    if ( AIDA::IHistogram1D* number_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[_numberTracksLocalname]) )
      number_histo->fill(_nGoodTracks);
    else {
      streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _numberTracksLocalname << endl;
      streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
      _histogramSwitch = false;
    }
  }

#endif

  // count events
  ++_iEvt;
  if ( isFirstEvent() ) _isFirstEvent = false;

}


int EUTelMille::guessSensorID( TrackerHitImpl * hit ) {
  if(hit==0)
{
    streamlog_out( ERROR ) << "An invalid hit pointer supplied! will exit now\n" << endl;
    return -1;
}

        try
        {
            LCObjectVec clusterVector = hit->getRawHits();

            EUTelVirtualCluster * cluster=0;

            if ( hit->getType() == kEUTelBrickedClusterImpl ) {

               // fixed cluster implementation. Remember it
               //  can come from
               //  both RAW and ZS data
   
                cluster = new EUTelBrickedClusterImpl(static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
                
            } else if ( hit->getType() == kEUTelDFFClusterImpl ) {
              
              // fixed cluster implementation. Remember it can come from
              // both RAW and ZS data
              cluster = new EUTelDFFClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
            } else if ( hit->getType() == kEUTelFFClusterImpl ) {
              
              // fixed cluster implementation. Remember it can come from
              // both RAW and ZS data
              cluster = new EUTelFFClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
            } 
            else if ( hit->getType() == kEUTelAPIXClusterImpl ) 
            {
              
//              cluster = new EUTelSparseClusterImpl< EUTelAPIXSparsePixel >
//                 ( static_cast<TrackerDataImpl *> ( clusterVector[ 0 ]  ) );

                // streamlog_out(MESSAGE4) << "Type is kEUTelAPIXClusterImpl" << endl;
                TrackerDataImpl * clusterFrame = static_cast<TrackerDataImpl*> ( clusterVector[0] );
                // streamlog_out(MESSAGE4) << "Vector size is: " << clusterVector.size() << endl;

                cluster = new eutelescope::EUTelSparseClusterImpl< eutelescope::EUTelAPIXSparsePixel >(clusterFrame);
	      
	        // CellIDDecoder<TrackerDataImpl> cellDecoder(clusterFrame);
                eutelescope::EUTelSparseClusterImpl< eutelescope::EUTelAPIXSparsePixel > *apixCluster = new eutelescope::EUTelSparseClusterImpl< eutelescope::EUTelAPIXSparsePixel >(clusterFrame);
                
            }
            else if ( hit->getType() == kEUTelSparseClusterImpl ) 
            {
               cluster = new EUTelSparseClusterImpl< EUTelSimpleSparsePixel > ( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
            }

            if(cluster != 0)
            {
              int sensorID = cluster->getDetectorID();
//              printf("actual sensorID: %5d \n", sensorID );
              return sensorID;
            }
          }
          catch(...)
          {
            printf("guess SensorID crashed \n");
          }

return -1;
}

int EUTelMille::guessSensorID( double * hit ) 
{

  int sensorID = -1;
  double minDistance =  numeric_limits< double >::max() ;
//  double * hitPosition = const_cast<double * > (hit->getPosition());

//  LCCollectionVec * referenceHitVec     = dynamic_cast < LCCollectionVec * > (evt->getCollection( _referenceHitCollectionName));
  if( _referenceHitVec == 0 || _applyToReferenceHitCollection == false)
  {
    // use z information of planes instead of reference vector
    for ( int iPlane = 0 ; iPlane < _siPlanesLayerLayout->getNLayers(); ++iPlane ) {
      double distance = std::abs( hit[2] - _siPlaneZPosition[ iPlane ] );
      if ( distance < minDistance ) {
	minDistance = distance;
	sensorID = _siPlanesLayerLayout->getID( iPlane );
      }
    }
    if ( minDistance > 30  ) {
      // advice the user that the guessing wasn't successful 
      streamlog_out( WARNING3 ) << "A hit was found " << minDistance << " mm far from the nearest plane\n"
	"Please check the consistency of the data with the GEAR file: hitPosition[2]=" << hit[2] <<       endl;
    }
    
    return sensorID;
  }

      for(size_t ii = 0 ; ii <  _referenceHitVec->getNumberOfElements(); ii++)
      {
        EUTelReferenceHit* refhit = static_cast< EUTelReferenceHit*> ( _referenceHitVec->getElementAt(ii) ) ;
//        printf(" _referenceHitVec %p refhit %p at %5.2f %5.2f %5.2f wih %5.2f %5.2f %5.2f \n", _referenceHitVec, refhit,
//                refhit->getXOffset(), refhit->getYOffset(), refhit->getZOffset(),
//                refhit->getAlpha(), refhit->getBeta(), refhit->getGamma()  
//               );
        
        TVector3 hit3d( hit[0], hit[1], hit[2] );
        TVector3 hitInPlane( refhit->getXOffset(), refhit->getYOffset(), refhit->getZOffset());
        TVector3 norm2Plane( refhit->getAlpha(), refhit->getBeta(), refhit->getGamma() );
 
        double distance = abs( norm2Plane.Dot(hit3d-hitInPlane) );
//          printf("iPlane %5d   hitPos:  [%8.3f;%8.3f%8.3f]  distance: %8.3f \n", refhit->getSensorID(), hit[0],hit[1],hit[2], distance  );
        if ( distance < minDistance ) 
        {
           minDistance = distance;
           sensorID = refhit->getSensorID();
//           printf("sensorID: %5d \n", sensorID );
        }    

      }

  return sensorID;
}


TVector3 EUTelMille::Line2Plane(int iplane, const TVector3& lpoint, const TVector3& lvector ) 
{

  if( _referenceHitVec == 0)
  {
    streamlog_out(MESSAGE) << "_referenceHitVec is empty" << endl;
    return TVector3(0.,0.,0.);
  }

        EUTelReferenceHit* refhit = static_cast< EUTelReferenceHit*> ( _referenceHitVec->getElementAt(iplane) ) ;
        
        TVector3 hitInPlane( refhit->getXOffset()*1000., refhit->getYOffset()*1000., refhit->getZOffset()*1000.); // go back to mm
        TVector3 norm2Plane( refhit->getAlpha(), refhit->getBeta(), refhit->getGamma() );
        TVector3 point( 1.,1.,1. );
          
        double linecoord_numenator   = norm2Plane.Dot(hitInPlane-lpoint);
        double linecoord_denumenator = norm2Plane.Dot(lvector);
//        printf("line: numenator: %8.3f denum: %8.3f [%5.3f %5.3f %5.3f::%5.3f %5.3f %5.3f]\n", linecoord_numenator, linecoord_denumenator, norm2Plane[0], norm2Plane[1], norm2Plane[2], lvector[0], lvector[1], lvector[2] );

        point = (linecoord_numenator/linecoord_denumenator)*lvector + lpoint;
//        double distance = abs( norm2Plane.Dot(hit3d-hitInPlane) );
//      printf("iPlane %5d   hitTrack:  [%8.3f;%8.3f;%13.3f] lvector[%5.2f %5.2f %5.2f] -> point[%5.2f %5.2f %13.2f]\n",
//                                       refhit->getSensorID(), 
//                                       hitInPlane[0], hitInPlane[1], hitInPlane[2],
//                                       lvector[0], lvector[1], lvector[2],
//                                       point[0], point[1], point[2]  );

  return point;
}


      
bool EUTelMille::hitContainsHotPixels( TrackerHitImpl   * hit) 
{
  bool skipHit = false;

  try
    {
      try{
	LCObjectVec clusterVector = hit->getRawHits();

	EUTelVirtualCluster * cluster;

	if ( hit->getType() == kEUTelSparseClusterImpl ) 
	  {
      
	    TrackerDataImpl * clusterFrame = dynamic_cast<TrackerDataImpl*> ( clusterVector[0] );
	    if (clusterFrame == 0){
	      // found invalid result from cast
	      throw UnknownDataTypeException("Invalid hit found in method hitContainsHotPixels()");
	    }
		
	    eutelescope::EUTelSparseClusterImpl< eutelescope::EUTelSimpleSparsePixel > *cluster = new eutelescope::EUTelSparseClusterImpl< eutelescope::EUTelSimpleSparsePixel >(clusterFrame);
	    int sensorID = cluster->getDetectorID();
 
	    for ( unsigned int iPixel = 0; iPixel < cluster->size(); iPixel++ ) 
              {
                EUTelSimpleSparsePixel m26Pixel;
                cluster->getSparsePixelAt( iPixel, &m26Pixel);
                int pixelX, pixelY;
                pixelX = m26Pixel.getXCoord();
                pixelY = m26Pixel.getYCoord();
		//                streamlog_out ( MESSAGE ) << iPixel << " of " << cluster->size() << "real Pixel:  " << pixelX << " " << pixelY << " " <<  endl;
 
                try
		  {                       
		    char ix[100];
		    sprintf(ix, "%d,%d,%d", sensorID, pixelX, pixelY ); 
		    //                       printf("real pixel at %s \n", ix);
		    //
		    //  std::map< std::string, bool >::iterator iter = _hotPixelMap.begin();
		    //  while ( iter != _hotPixelMap.end() ) {
		    //    std::cout << "Found hot pixel " << iter->second << " at " << iter->first << std::endl;
		    //    ++iter;
		    //  }
   
		    //                       std::cout << _hotPixelMap.first() << " " << _hotPixelMap.second() << std::endl; 
		    std::map<std::string, bool >::const_iterator z = _hotPixelMap.find(ix);
		    if(z!=_hotPixelMap.end() && _hotPixelMap[ix] == true  )
		      { 
			skipHit = true; 	      
			//                          streamlog_out(MESSAGE4) << "Skipping hit due to hot pixel content." << endl;
			//                          printf("pixel %3d %3d was found in the _hotPixelMap \n", pixelX, pixelY  );
			return true; // if TRUE  this hit will be skipped
		      }
		    else
		      { 
			skipHit = false; 	      
		      } 
		  } 
		catch (...)
		  {
		    //                       printf("pixel %3d %3d was NOT found in the _hotPixelMap \n", pixelX, pixelY  );
		  }
	      }


	  } else if ( hit->getType() == kEUTelBrickedClusterImpl ) {

	  // fixed cluster implementation. Remember it
	  //  can come from
	  //  both RAW and ZS data
   
	  cluster = new EUTelBrickedClusterImpl(static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
                
	} else if ( hit->getType() == kEUTelDFFClusterImpl ) {
              
	  // fixed cluster implementation. Remember it can come from
	  // both RAW and ZS data
	  cluster = new EUTelDFFClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
	} else if ( hit->getType() == kEUTelFFClusterImpl ) {
              
	  // fixed cluster implementation. Remember it can come from
	  // both RAW and ZS data
	  cluster = new EUTelFFClusterImpl( static_cast<TrackerDataImpl *> ( clusterVector[0] ) );
	} 
	else if ( hit->getType() == kEUTelAPIXClusterImpl ) 
	  {
              
	    //              cluster = new EUTelSparseClusterImpl< EUTelAPIXSparsePixel >
	    //                 ( static_cast<TrackerDataImpl *> ( clusterVector[ 0 ]  ) );

	    // streamlog_out(MESSAGE4) << "Type is kEUTelAPIXClusterImpl" << endl;
	    TrackerDataImpl * clusterFrame = static_cast<TrackerDataImpl*> ( clusterVector[0] );
	    // streamlog_out(MESSAGE4) << "Vector size is: " << clusterVector.size() << endl;

	    cluster = new eutelescope::EUTelSparseClusterImpl< eutelescope::EUTelAPIXSparsePixel >(clusterFrame);
	      
	    // CellIDDecoder<TrackerDataImpl> cellDecoder(clusterFrame);
	    eutelescope::EUTelSparseClusterImpl< eutelescope::EUTelAPIXSparsePixel > *apixCluster = new eutelescope::EUTelSparseClusterImpl< eutelescope::EUTelAPIXSparsePixel >(clusterFrame);
                
	    int sensorID = apixCluster->getDetectorID();//static_cast<int> ( cellDecoder(clusterFrame)["sensorID"] );
	    //                cout << "Pixels at sensor " << sensorID << ": ";

	    //                bool skipHit = 0;
	    for (int iPixel = 0; iPixel < apixCluster->size(); ++iPixel) 
	      {
		int pixelX, pixelY;
		EUTelAPIXSparsePixel apixPixel;
		apixCluster->getSparsePixelAt(iPixel, &apixPixel);
		pixelX = apixPixel.getXCoord();
		pixelY = apixPixel.getYCoord();
		//                    cout << "(" << pixelX << "|" << pixelY << ") ";
		//                    cout << endl;

		try
		  {                       
		    //                       printf("pixel %3d %3d was found in the _hotPixelMap = %1d (0/1) \n", pixelX, pixelY, _hotPixelMap[sensorID][pixelX][pixelY]  );                       
		    char ix[100];
		    sprintf(ix, "%d,%d,%d", sensorID, apixPixel.getXCoord(), apixPixel.getYCoord() ); 
		    std::map<std::string, bool >::const_iterator z = _hotPixelMap.find(ix);
		    if(z!=_hotPixelMap.end() && _hotPixelMap[ix] == true  )
		      { 
			skipHit = true; 	      
			//                          streamlog_out(MESSAGE4) << "Skipping hit due to hot pixel content." << endl;
			//                          printf("pixel %3d %3d was found in the _hotPixelMap \n", pixelX, pixelY  );
			return true; // if TRUE  this hit will be skipped
		      }
		    else
		      { 
			skipHit = false; 	      
		      } 
		  } 
		catch (...)
		  {
		    //                       printf("pixel %3d %3d was NOT found in the _hotPixelMap \n", pixelX, pixelY  );
		  }
           
		//                    skipHit = skipHit || hitContainsHotPixels(hit);

		if(skipHit ) 
		  {
		    //                       printf("pixel %3d %3d was found in the _hotPixelMap \n", pixelX, pixelY  );
		  }
		else
		  { 
		    //                       printf("pixel %3d %3d was NOT found in the _hotPixelMap \n", pixelX, pixelY  );
		  }

	      }

	    if (skipHit) 
	      {
		//                      streamlog_out(MESSAGE4) << "Skipping hit due to hot pixel content." << endl;
		//                    continue;
	      }
	    else
	      {
		//                    streamlog_out(MESSAGE4) << "Cluster/hit is fine for preAlignment!" << endl;
	      }


	    return skipHit; // if TRUE  this hit will be skipped
	  } 
            
      }
      catch(lcio::Exception e){
	// catch specific exceptions
          streamlog_out ( ERROR ) << "Exception occured in hitContainsHotPixels(): " << e.what() << endl;
      }
    }
  catch(...)
    { 
      // if anything went wrong in the above return FALSE, meaning do not skip this hit
      return 0;
    }

  // if none of the above worked return FALSE, meaning do not skip this hit
  return 0;

}


void EUTelMille::end() {

//printf("end() \n");

  delete [] _telescopeResolY;
  delete [] _telescopeResolX;
  delete [] _telescopeResolZ;
  delete [] _yFitPos;
  delete [] _xFitPos;
  delete [] _waferResidY;
  delete [] _waferResidX;
  delete [] _waferResidZ;

//printf("delete hitsarray \n");

#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
  if(_alignMode == 3)
    {
      delete []  hitsarray;
    }
#endif

//printf("mille \n");

  // close the output file
  delete _mille;

//printf("mille OK\n");


  // if write the pede steering file
  if (_generatePedeSteerfile) {

    streamlog_out ( MESSAGE4 ) << endl << "Generating the steering file for the pede program..." << endl;

    string tempHistoName;
    double *meanX = new double[_nPlanes];
    double *meanY = new double[_nPlanes];
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
    double *meanZ = new double[_nPlanes];
#endif

//printf("loop over all detector planes \n");
    // loop over all detector planes
    for( int iDetector = 0; iDetector < _nPlanes; iDetector++ ) {

      int sensorID = _orderedSensorID.at( iDetector );

      if ( _histogramSwitch ) {
        tempHistoName =  _residualXLocalname + "_d" + to_string( sensorID );
        if ( AIDA::IHistogram1D* residx_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[tempHistoName.c_str()]) )
          meanX[iDetector] = residx_histo->mean();
        else {
          streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _residualXLocalname << endl;
          streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
          _histogramSwitch = false;
        }
      }

      if ( _histogramSwitch ) {
        tempHistoName =  _residualYLocalname + "_d" + to_string( sensorID );
        if ( AIDA::IHistogram1D* residy_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[tempHistoName.c_str()]) )
          meanY[iDetector] = residy_histo->mean();
        else {
          streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _residualYLocalname << endl;
          streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
          _histogramSwitch = false;
        }
      }
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
      if ( _histogramSwitch ) {
        tempHistoName =  _residualZLocalname + "_d" + to_string( sensorID );
        if ( AIDA::IHistogram1D* residz_histo = dynamic_cast<AIDA::IHistogram1D*>(_aidaHistoMap[tempHistoName.c_str()]) )
          meanZ[iDetector] = residz_histo->mean();
        else {
          streamlog_out ( ERROR2 ) << "Not able to retrieve histogram pointer for " << _residualZLocalname << endl;
          streamlog_out ( ERROR2 ) << "Disabling histogramming from now on" << endl;
          _histogramSwitch = false;
        } 
      }
#endif
    } // end loop over all detector planes

    ofstream steerFile;
    steerFile.open(_pedeSteerfileName.c_str());

    if (steerFile.is_open()) {

      // find first and last excluded plane
      int firstnotexcl = _nPlanes;
      int lastnotexcl = 0;

      // loop over all planes
      for (int help = 0; help < _nPlanes; help++) 
      {

        int excluded = 0;

        // loop over all excluded planes
        for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) 
        {
          if (help == _excludePlanes[helphelp]) 
          {
//            excluded = 1;
          }
        } // end loop over all excluded planes

        if (excluded == 0 && firstnotexcl > help) 
        {
          firstnotexcl = help;
        }

        if (excluded == 0 && lastnotexcl < help) 
        {
          lastnotexcl = help;
        }
      } // end loop over all planes

      // calculate average
      double averageX = (meanX[firstnotexcl] + meanX[lastnotexcl]) / 2;
      double averageY = (meanY[firstnotexcl] + meanY[lastnotexcl]) / 2;
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
      double averageZ = (meanZ[firstnotexcl] + meanZ[lastnotexcl]) / 2;
#endif
      steerFile << "Cfiles" << endl;
      steerFile << _binaryFilename << endl;
      steerFile << endl;

      steerFile << "Parameter" << endl;

      int counter = 0;

      // loop over all planes
      for (int help = 0; help < _nPlanes; help++) {
//        printf("end: plane %5d of %5d \n", help, _nPlanes);
 
        int excluded = 0; // flag for excluded planes

        // loop over all excluded planes
        for (int helphelp = 0; helphelp < _nExcludePlanes; helphelp++) {

          if (help == _excludePlanes[helphelp]) {
//            excluded = 1;
          }

        } // end loop over all excluded planes

        // if plane not excluded
        if (excluded == 0) {
          
          bool fixed = false;
          for(size_t i = 0;i< _FixedPlanes.size(); i++)
            {
              if(_FixedPlanes[i] == help)
                fixed = true;
            }
          
          // if fixed planes
          // if (help == firstnotexcl || help == lastnotexcl) {
          if( fixed || (_FixedPlanes.size() == 0 && (help == firstnotexcl || help == lastnotexcl) ) )
            {
              if (_alignMode == 1) {
                steerFile << (counter * 3 + 1) << " 0.0 -1.0" << endl;
                steerFile << (counter * 3 + 2) << " 0.0 -1.0" << endl;
                steerFile << (counter * 3 + 3) << " 0.0 -1.0" << endl;
              } else if (_alignMode == 2) {
                steerFile << (counter * 2 + 1) << " 0.0 -1.0" << endl;
                steerFile << (counter * 2 + 2) << " 0.0 -1.0" << endl;
              } else if (_alignMode == 3) {
                steerFile << (counter * 6 + 1) << " 0.0 -1.0" << endl;
                steerFile << (counter * 6 + 2) << " 0.0 -1.0" << endl;
                steerFile << (counter * 6 + 3) << " 0.0 -1.0" << endl;
                steerFile << (counter * 6 + 4) << " 0.0 -1.0" << endl;
                steerFile << (counter * 6 + 5) << " 0.0 -1.0" << endl;
                steerFile << (counter * 6 + 6) << " 0.0 -1.0" << endl;
              }
              
            } else {
            
            if (_alignMode == 1) {

              if (_usePedeUserStartValues == 0) {
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
                steerFile << (counter * 3 + 1) << " " << (averageX - meanX[help]) << " 0.0" << endl;
                steerFile << (counter * 3 + 2) << " " << (averageY - meanY[help]) << " 0.0" << endl;
                steerFile << (counter * 3 + 3) << " " << " 0.0 0.0" << endl;
#endif
              } else {
                steerFile << (counter * 3 + 1) << " " << _pedeUserStartValuesX[help] << " 0.0" << endl;
                steerFile << (counter * 3 + 2) << " " << _pedeUserStartValuesY[help] << " 0.0" << endl;
                steerFile << (counter * 3 + 3) << " " << _pedeUserStartValuesGamma[help] << " 0.0" << endl;
              }

            } else if (_alignMode == 2) {

              if (_usePedeUserStartValues == 0) {
                steerFile << (counter * 2 + 1) << " " << (averageX - meanX[help]) << " 0.0" << endl;
                steerFile << (counter * 2 + 2) << " " << (averageY - meanY[help]) << " 0.0" << endl;
              } else {
                steerFile << (counter * 2 + 1) << " " << _pedeUserStartValuesX[help] << " 0.0" << endl;
                steerFile << (counter * 2 + 2) << " " << _pedeUserStartValuesY[help] << " 0.0" << endl;
              }

            } else if (_alignMode == 3) {
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
              if (_usePedeUserStartValues == 0)
                {
                  if(_FixParameter[help] & (1 << 0))
                    steerFile << (counter * 6 + 1) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 1) << " " << (averageX - meanX[help]) << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 1))
                    steerFile << (counter * 6 + 2) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 2) << " " << (averageY - meanY[help]) << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 2))
                    steerFile << (counter * 6 + 3) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 3) << " " << (averageZ - meanZ[help]) << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 3))
                    steerFile << (counter * 6 + 4) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 4) << " 0.0 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 4))
                    steerFile << (counter * 6 + 5) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 5) << " 0.0 0.0" << endl;

                  if(_FixParameter[help] & (1 << 5))
                    steerFile << (counter * 6 + 6) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 6) << " 0.0 0.0" << endl;
                }
              else
                {
                  if(_FixParameter[help] & (1 << 0))
                    steerFile << (counter * 6 + 1) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 1) << " " << _pedeUserStartValuesX[help] << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 1))
                    steerFile << (counter * 6 + 2) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 2) << " " << _pedeUserStartValuesY[help] << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 2))
                    steerFile << (counter * 6 + 3) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 3) << " " << _pedeUserStartValuesZ[help] << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 3))
                    steerFile << (counter * 6 + 4) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 4) << " " << _pedeUserStartValuesAlpha[help] << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 4))
                    steerFile << (counter * 6 + 5) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 5) << " " << _pedeUserStartValuesBeta[help] << " 0.0" << endl;
                  
                  if(_FixParameter[help] & (1 << 5))
                    steerFile << (counter * 6 + 6) << " 0.0 -1.0" << endl;
                  else
                    steerFile << (counter * 6 + 6) << " " << _pedeUserStartValuesGamma[help] << " 0.0" << endl;
                }
#endif
            }
          }

          counter++;

        } // end if plane not excluded

      } // end loop over all planes

      steerFile << endl;
      steerFile << "! chiscut 5.0 2.5" << endl;
      steerFile << "! outlierdownweighting 4" << endl;
      steerFile << endl;
      steerFile << "method inversion 10 0.001" << endl;
      steerFile << endl;
      steerFile << "histprint" << endl;
      steerFile << endl;
      steerFile << "end" << endl;

      steerFile.close();

      streamlog_out ( MESSAGE2 ) << "File " << _pedeSteerfileName << " written." << endl;

    } else {

      streamlog_out ( ERROR2 ) << "Could not open steering file." << endl;

    }
    //cleaning up
    delete [] meanX;
    delete [] meanY;
#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
    delete [] meanZ;
#endif
  } // end if write the pede steering file

  streamlog_out ( MESSAGE2 ) << endl;
  streamlog_out ( MESSAGE2 ) << "Number of data points used: " << _nMilleDataPoints << endl;
  streamlog_out ( MESSAGE2 ) << "Number of tracks used: " << _nMilleTracks << endl;

  // if running pede using the generated steering file
  if (_runPede == 1) {

    // check if steering file exists
    if (_generatePedeSteerfile == 1) {

      std::string command = "pede " + _pedeSteerfileName;

      streamlog_out ( MESSAGE2 ) << endl;
      streamlog_out ( MESSAGE2 ) << "Starting pede..." << endl;
      streamlog_out ( MESSAGE2 ) << command.c_str() << endl;

      bool encounteredError = false;
      
      // run pede and create a streambuf that reads its stdout and stderr
      redi::ipstream pede( command.c_str(), redi::pstreams::pstdout|redi::pstreams::pstderr ); 
      
      if (!pede.is_open()) {
	  streamlog_out( ERROR ) << "Pede cannot be executed: command not found in the path" << endl;
	  encounteredError = true;	  
      } else {

	// output multiplexing: parse pede output in both stdout and stderr and echo messages accordingly
	char buf[1024];
	std::streamsize n;
	std::stringstream pedeoutput; // store stdout to parse later
	std::stringstream pedeerrors;
	bool finished[2] = { false, false };
	while (!finished[0] || !finished[1])
	  {
	    if (!finished[0])
	      {
		while ((n = pede.err().readsome(buf, sizeof(buf))) > 0){
		  streamlog_out( ERROR ).write(buf, n).flush();
		  string error (buf, n);
		  pedeerrors << error;
		  encounteredError = true;
		}
		if (pede.eof())
		  {
		    finished[0] = true;
		    if (!finished[1])
		      pede.clear();
		  }
	      }

	    if (!finished[1])
	      {
		while ((n = pede.out().readsome(buf, sizeof(buf))) > 0){
		  streamlog_out( MESSAGE2 ).write(buf, n).flush();
		  string output (buf, n);
		  pedeoutput << output;
		}
		if (pede.eof())
		  {
		    finished[1] = true;
		    if (!finished[0])
		      pede.clear();
		  }
	      }
	  }

	// pede does not return exit codes on some errors (in V03-04-00)
	// check for some of those here by parsing the output
	const char * pch = strstr(pedeoutput.str().data(),"Too many rejects");
	if (pch){
	  streamlog_out ( ERROR ) << "Pede stopped due to the large number of rejects. " << endl;
	  encounteredError = true;
	}

        // wait for the pede execution to finish
        pede.close();

        // check the exit value of pede / react to previous errors
        if ( pede.rdbuf()->status() == 0 && !encounteredError) 
        {
          streamlog_out ( MESSAGE2 ) << "Pede successfully finished" << endl;
        } else {
          streamlog_out ( ERROR ) << "Problem during Pede execution, exit status: " << pede.rdbuf()->status() << ", error messages (repeated here): " << endl;
	  streamlog_out ( ERROR4 ) << pedeerrors.str() << endl;
	  // TODO: decide what to do now; exit? and if, how?
          streamlog_out ( ERROR4 ) << "Will exit now" << endl;
	  //exit(EXIT_FAILURE); // FIXME: can lead to (ROOT?) seg faults - points to corrupt memory? run valgrind...
	  return; // does fine for now
	}

        // reading back the millepede.res file and getting the
        // results.
        string millepedeResFileName = "millepede.res";

        streamlog_out ( MESSAGE2 ) << "Reading back the " << millepedeResFileName << endl
                                   << "Saving the alignment constant into " << _alignmentConstantLCIOFile << endl;

        // open the millepede ASCII output file
        ifstream millepede( millepedeResFileName.c_str() );


        // reopen the LCIO file this time in append mode
        LCWriter * lcWriter = LCFactory::getInstance()->createLCWriter();

        try 
        {
          lcWriter->open( _alignmentConstantLCIOFile, LCIO::WRITE_NEW );
        }
        catch ( IOException& e ) 
        {
          streamlog_out ( ERROR4 ) << e.what() << endl
                                   << "Sorry for quitting. " << endl;
          exit(-1);
        }


        // write an almost empty run header
        LCRunHeaderImpl * lcHeader  = new LCRunHeaderImpl;
        lcHeader->setRunNumber( 0 );


        lcWriter->writeRunHeader(lcHeader);

        delete lcHeader;

        LCEventImpl * event = new LCEventImpl;
        event->setRunNumber( 0 );
        event->setEventNumber( 0 );

        LCTime * now = new LCTime;
        event->setTimeStamp( now->timeStamp() );
        delete now;

        LCCollectionVec * constantsCollection = new LCCollectionVec( LCIO::LCGENERICOBJECT );


        if ( millepede.bad() || !millepede.is_open() ) 
        {
          streamlog_out ( ERROR4 ) << "Error opening the " << millepedeResFileName << endl
                                   << "The alignment slcio file cannot be saved" << endl;
        }
        else 
        {
          vector<double > tokens;
          stringstream tokenizer;
          string line;
          double buffer;

          // get the first line and throw it away since it is a
          // comment!
          getline( millepede, line );
          std::cout << "line:" <<  line  << std::endl;

          int counter = 0;

          while ( ! millepede.eof() ) {

            EUTelAlignmentConstant * constant = new EUTelAlignmentConstant;

            bool goodLine = true;
            unsigned int numpars = 0;
            if(_alignMode != 3)
              numpars = 3;
            else
              numpars = 6;

            bool _nonzero_tokens = false;



            for ( unsigned int iParam = 0 ; iParam < numpars ; ++iParam ) 
            {
              getline( millepede, line );

              if ( line.empty() ) {
                goodLine = false;
                continue;
              }



              tokens.clear();
              tokenizer.clear();
              tokenizer.str( line );

              // check that all parts of the line are non zero
//              int ibuff=0;
              while ( tokenizer >> buffer ) {
                tokens.push_back( buffer ) ;
                if(buffer> 1e-12) _nonzero_tokens = true;
//                ibuff++;
//                printf("buff[%2d:%1d] = %8.3f ", iParam, ibuff, buffer);
              }
//              printf("\n");

              if ( ( tokens.size() == 3 ) || ( tokens.size() == 6 ) || (tokens.size() == 5) ) {
                goodLine = true;
              } else goodLine = false;

              bool isFixed = ( tokens.size() == 3 );
/*
              if ( isFixed ) {
                streamlog_out ( MESSAGE ) << "Parameter " << tokens[0] << " is at " << ( tokens[1] / 1000 )
                                         << " (fixed)"  << endl;
              } else {
                streamlog_out ( MESSAGE ) << "Parameter " << tokens[0] << " is at " << (tokens[1] / 1000 )
                                         << " +/- " << ( tokens[4] / 1000 )  << endl;
              }
*/
              if(_alignMode != 3)
                {
                 if ( iParam == 0 ) {
                    constant->setXOffset( tokens[1] / 1000 );
                    if ( ! isFixed ) constant->setXOffsetError( tokens[4] / 1000 ) ;
                  }
                  if ( iParam == 1 ) {
                    constant->setYOffset( tokens[1] / 1000 ) ;
                    if ( ! isFixed ) constant->setYOffsetError( tokens[4] / 1000 ) ;
                  }
                  if ( iParam == 2 ) {
                    constant->setGamma( tokens[1]  ) ;
                    if ( ! isFixed ) constant->setGammaError( tokens[4] ) ;
                  }
                }
              else
                {
                 if ( iParam == 0 ) {
                    constant->setXOffset( tokens[1] / 1000 );
                    if ( ! isFixed ) constant->setXOffsetError( tokens[4] / 1000 ) ;                    
                  }
                  if ( iParam == 1 ) {
                    constant->setYOffset( tokens[1] / 1000 ) ;
                    if ( ! isFixed ) constant->setYOffsetError( tokens[4] / 1000 ) ;
                  }
                  if ( iParam == 2 ) {
                    constant->setZOffset( tokens[1] / 1000 ) ;
                    if ( ! isFixed ) constant->setZOffsetError( tokens[4] / 1000 ) ;
                  }
                  if ( iParam == 3 ) {
                    constant->setAlpha( tokens[1]  ) ;
                    if ( ! isFixed ) constant->setAlphaError( tokens[4] ) ;
                  } 
                  if ( iParam == 4 ) {
                    constant->setBeta( tokens[1]  ) ;
                    if ( ! isFixed ) constant->setBetaError( tokens[4] ) ;
                  } 
                  if ( iParam == 5 ) {
                    constant->setGamma( tokens[1]  ) ;
                    if ( ! isFixed ) constant->setGammaError( tokens[4] ) ;
                  } 

                }
              
            }


//printf("goodLine : %1d \n", goodLine, _orderedSensorID.at( counter ) );
            // right place to add the constant to the collection
            if ( goodLine  ) {
//               constant->setSensorID( _orderedSensorID_wo_excluded.at( counter ) );
              constant->setSensorID( _orderedSensorID.at( counter ) );
              ++ counter;
              constantsCollection->push_back( constant );
              streamlog_out ( MESSAGE0 ) << (*constant) << endl;
            }
            else delete constant;
          }

        }



        event->addCollection( constantsCollection, _alignmentConstantCollectionName );
        lcWriter->writeEvent( event );
        delete event;

        lcWriter->close();

        millepede.close();

      }
    } else {

      streamlog_out ( ERROR2 ) << "Unable to run pede. No steering file has been generated." << endl;

    }


  } // end if running pede using the generated steering file

  streamlog_out ( MESSAGE2 ) << endl;
  streamlog_out ( MESSAGE2 ) << "Successfully finished" << endl;
}

void EUTelMille::bookHistos() {


#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)

  try {
    streamlog_out ( MESSAGE2 ) << "Booking histograms..." << endl;

    const int    tracksNBin = 20  ;
    const double tracksMin  = -0.5;
    const double tracksMax  = 19.5;
    const int    Chi2NBin = 1000 ;
    const double Chi2Min  =   0.;
    const double Chi2Max  = 1000.;
    const int    NBin =   4000;
    const double Min  = -2000.;
    const double Max  =  2000.;

    AIDA::IHistogram1D * numberTracksLocal =
      AIDAProcessor::histogramFactory(this)->createHistogram1D(_numberTracksLocalname,tracksNBin,tracksMin,tracksMax);
    if ( numberTracksLocal ) {
      numberTracksLocal->setTitle("Number of tracks after #chi^{2} cut");
      _aidaHistoMap.insert( make_pair( _numberTracksLocalname, numberTracksLocal ) );
    } else {
      streamlog_out ( ERROR2 ) << "Problem booking the " << (_numberTracksLocalname) << endl;
      streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
      _histogramSwitch = false;
    }

    AIDA::IHistogram1D * chi2XLocal =
      AIDAProcessor::histogramFactory(this)->createHistogram1D(_chi2XLocalname,Chi2NBin,Chi2Min,Chi2Max);
    if ( chi2XLocal ) {
      chi2XLocal->setTitle("Chi2 X");
      _aidaHistoMap.insert( make_pair( _chi2XLocalname, chi2XLocal ) );
    } else {
      streamlog_out ( ERROR2 ) << "Problem booking the " << (_chi2XLocalname) << endl;
      streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
      _histogramSwitch = false;
    }

    AIDA::IHistogram1D * chi2YLocal =
      AIDAProcessor::histogramFactory(this)->createHistogram1D(_chi2YLocalname,Chi2NBin,Chi2Min,Chi2Max);
    if ( chi2YLocal ) {
      chi2YLocal->setTitle("Chi2 Y");
      _aidaHistoMap.insert( make_pair( _chi2YLocalname, chi2YLocal ) );
    } else {
      streamlog_out ( ERROR2 ) << "Problem booking the " << (_chi2YLocalname) << endl;
      streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
      _histogramSwitch = false;
    }

    string tempHistoName;
    string histoTitleXResid;
    string histoTitleYResid;
    string histoTitleZResid;

    for( int iDetector = 0; iDetector < _nPlanes; iDetector++ ){

      // this is the sensorID corresponding to this plane
      int sensorID = _orderedSensorID.at( iDetector );

      tempHistoName     =  _residualXLocalname + "_d" + to_string( sensorID );
      histoTitleXResid  =  "XResidual_d" + to_string( sensorID ) ;

      AIDA::IHistogram1D *  tempXHisto =
        AIDAProcessor::histogramFactory(this)->createHistogram1D(tempHistoName,NBin, Min,Max);
      if ( tempXHisto ) {
        tempXHisto->setTitle(histoTitleXResid);
        _aidaHistoMap.insert( make_pair( tempHistoName, tempXHisto ) );
      } else {
        streamlog_out ( ERROR2 ) << "Problem booking the " << (tempHistoName) << endl;
        streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
        _histogramSwitch = false;
      }

      tempHistoName     =  _residualXvsXLocalname + "_d" + to_string( sensorID );
      histoTitleXResid  =  "XvsXResidual_d" + to_string( sensorID ) ;

      AIDA::IProfile1D *  tempX2dHisto =
        AIDAProcessor::histogramFactory(this)->createProfile1D(tempHistoName, 100, -10000., 10000.,  -10000., 10000. );
      if ( tempX2dHisto ) {
        tempX2dHisto->setTitle(histoTitleXResid);
        _aidaHistoMapProf1D.insert( make_pair( tempHistoName, tempX2dHisto ) );
      } else {
        streamlog_out ( ERROR2 ) << "Problem booking the " << (tempHistoName) << endl;
        streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
        _histogramSwitch = false;
      }

      tempHistoName     =  _residualXvsYLocalname + "_d" + to_string( sensorID );
      histoTitleXResid  =  "XvsYResidual_d" + to_string( sensorID ) ;

       tempX2dHisto =
        AIDAProcessor::histogramFactory(this)->createProfile1D(tempHistoName,  100, -10000., 10000.,  -10000., 10000.);
      if ( tempX2dHisto ) {
        tempX2dHisto->setTitle(histoTitleXResid);
        _aidaHistoMapProf1D.insert( make_pair( tempHistoName, tempX2dHisto ) );
      } else {
        streamlog_out ( ERROR2 ) << "Problem booking the " << (tempHistoName) << endl;
        streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
        _histogramSwitch = false;
      }


      tempHistoName     =  _residualYLocalname + "_d" + to_string( sensorID );
      histoTitleYResid  =  "YResidual_d" + to_string( sensorID ) ;

      AIDA::IHistogram1D *  tempYHisto =
        AIDAProcessor::histogramFactory(this)->createHistogram1D(tempHistoName,NBin, Min,Max);
      if ( tempYHisto ) {
        tempYHisto->setTitle(histoTitleYResid);
        _aidaHistoMap.insert( make_pair( tempHistoName, tempYHisto ) );
      } else {
        streamlog_out ( ERROR2 ) << "Problem booking the " << (tempHistoName) << endl;
        streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
        _histogramSwitch = false;
      }

      tempHistoName     =  _residualYvsXLocalname + "_d" + to_string( sensorID );
      histoTitleYResid  =  "YvsXResidual_d" + to_string( sensorID ) ;

      AIDA::IProfile1D *  tempY2dHisto =
        AIDAProcessor::histogramFactory(this)->createProfile1D(tempHistoName, 100, -10000., 10000.,  -1000., 1000.);
      if ( tempY2dHisto ) {
        tempY2dHisto->setTitle(histoTitleYResid);
        _aidaHistoMapProf1D.insert( make_pair( tempHistoName, tempY2dHisto ) );
      } else {
        streamlog_out ( ERROR2 ) << "Problem booking the " << (tempHistoName) << endl;
        streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
        _histogramSwitch = false;
      }

      tempHistoName     =  _residualYvsYLocalname + "_d" + to_string( sensorID );
      histoTitleYResid  =  "YvsYResidual_d" + to_string( sensorID ) ;

        tempY2dHisto =
        AIDAProcessor::histogramFactory(this)->createProfile1D(tempHistoName, 100, -10000., 10000.,  -10000., 10000.);
      if ( tempY2dHisto ) {
        tempY2dHisto->setTitle(histoTitleYResid);
        _aidaHistoMapProf1D.insert( make_pair( tempHistoName, tempY2dHisto ) );
      } else {
        streamlog_out ( ERROR2 ) << "Problem booking the " << (tempHistoName) << endl;
        streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
        _histogramSwitch = false;
      }


#if defined(USE_ROOT) || defined(MARLIN_USE_ROOT)
      tempHistoName     =  _residualZLocalname + "_d" + to_string( sensorID );
      histoTitleZResid  =  "ZResidual_d" + to_string( sensorID ) ;

      AIDA::IHistogram1D *  tempZHisto =
        AIDAProcessor::histogramFactory(this)->createHistogram1D(tempHistoName,NBin, Min,Max);
      if ( tempZHisto ) {
        tempZHisto->setTitle(histoTitleZResid);
        _aidaHistoMap.insert( make_pair( tempHistoName, tempZHisto ) );
      } else {
        streamlog_out ( ERROR2 ) << "Problem booking the " << (tempHistoName) << endl;
        streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
        _histogramSwitch = false;
      }
 
 
      tempHistoName     =  _residualZvsYLocalname + "_d" + to_string( sensorID );
      histoTitleZResid  =  "ZvsYResidual_d" + to_string( sensorID ) ;

      AIDA::IProfile1D*  tempZ2dHisto =
        AIDAProcessor::histogramFactory(this)->createProfile1D(tempHistoName, 100, -10000., 10000.,  -10000., 10000.);
      if ( tempZ2dHisto ) {
        tempZ2dHisto->setTitle(histoTitleZResid);
        _aidaHistoMapProf1D.insert( make_pair( tempHistoName, tempZ2dHisto ) );
      } else {
        streamlog_out ( ERROR2 ) << "Problem booking the " << (tempHistoName) << endl;
        streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
        _histogramSwitch = false;
      }


      tempHistoName     =  _residualZvsXLocalname + "_d" + to_string( sensorID );
      histoTitleZResid  =  "ZvsXResidual_d" + to_string( sensorID ) ;

        tempZ2dHisto =
        AIDAProcessor::histogramFactory(this)->createProfile1D(tempHistoName, 100, -10000., 10000.,  -10000., 10000.);
      if ( tempZ2dHisto ) {
        tempZ2dHisto->setTitle(histoTitleZResid);
        _aidaHistoMapProf1D.insert( make_pair( tempHistoName, tempZ2dHisto ) );
      } else {
        streamlog_out ( ERROR2 ) << "Problem booking the " << (tempHistoName) << endl;
        streamlog_out ( ERROR2 ) << "Very likely a problem with path name. Switching off histogramming and continue w/o" << endl;
        _histogramSwitch = false;
      }


#endif
    }

  } catch (lcio::Exception& e ) {


#ifdef EUTEL_INTERACTIVE
    streamlog_out ( ERROR2 ) << "No AIDAProcessor initialized. Type q to exit or c to continue without histogramming" << endl;
    string answer;
    while ( true ) {
      streamlog_out ( ERROR2 ) << "[q]/[c]" << endl;
      cin >> answer;
      transform( answer.begin(), answer.end(), answer.begin(), ::tolower );
      if ( answer == "q" ) {
        exit(-1);
      } else if ( answer == "c" )
        _histogramSwitch = false;
      break;
    }
#else
    streamlog_out( WARNING2 ) << "No AIDAProcessor initialized. Continue without histogramming" << endl;

#endif


  }
#endif

}

#endif // USE_GEAR


