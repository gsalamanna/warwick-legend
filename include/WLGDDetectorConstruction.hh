#ifndef WLGDDetectorConstruction_h
#define WLGDDetectorConstruction_h 1

#include "G4Cache.hh"
#include "G4GenericMessenger.hh"
#include "G4VUserDetectorConstruction.hh"
#include "globals.hh"
#include "G4Material.hh"

class G4VPhysicalVolume;
class WLGDCrystalSD;

class WLGDDetectorConstruction : public G4VUserDetectorConstruction
{
public:
  WLGDDetectorConstruction();
  ~WLGDDetectorConstruction();

public:
  virtual G4VPhysicalVolume* Construct();
  virtual void               ConstructSDandField();

  G4double GetWorldSizeZ() { return fvertexZ; }  // inline
  G4double GetWorldExtent() { return fmaxrad; }  // --"--
  G4String GetGeometryName() { return fGeometryName; }
  G4int    isSetWithGdWater() {return fWithGdWater; }
  void     SetGeometry(const G4String& name);
  void     ExportGeometry(const G4String& file);
  void     SetNeutronBiasFactor(G4double nf);
  void     SetMuonBiasFactor(G4double mf);
  void     SetWithoutCupperTubes(G4int answer);
  void     SetBoratedPET(G4int answer);
  void     SetGdWater(G4int answer);
  void	   SetBoratedTurbineRadius(G4double radius);
  void     SetBoratedTurbineLength(G4double length);
  void     SetBoratedTurbineAngle(G4double deg);
  void     SetBoratedTurbineWidth(G4double width);
    void     SetBoratedTurbineHeight(G4double height);
    void     SetBoratedTurbinezPosition(G4double zPosition);

private:
  void DefineCommands();
  void DefineMaterials();

  G4VPhysicalVolume* SetupBaseline();
  G4VPhysicalVolume* SetupAlternative();
  G4VPhysicalVolume* SetupHallA();

  G4GenericMessenger*                 fDetectorMessenger = nullptr;
  G4GenericMessenger*                 fBiasMessenger     = nullptr;
  G4double                            fvertexZ           = -1.0;
  G4double                            fmaxrad            = -1.0;
  G4String                            fGeometryName      = "baseline";
  G4double                            fNeutronBias       = 1.0;
  G4double                            fMuonBias          = 1.0;
  G4Cache<WLGDCrystalSD*> fSD                = nullptr;
  G4double                            fXeConc            = 0.0;
  G4double                            fHe3Conc            = 0.0;
  G4double			      fCryostatOuterRadius = 350.0;
  G4double 			      fCryostatHeight = 350.0;
  G4double                            fBoratedTurbineRadius = 200.0;
    G4double                            fBoratedTurbineLength = 50.0;
    G4double                            fBoratedTurbineAngle = 45.0;
    G4double                            fBoratedTurbineWidth = 5.0;
    G4double                            fBoratedTurbineHeight = 600.;
    G4double                            fBoratedTurbinezPosition = 0.;
  G4int                               fWithOutCupperTubes = 0;
  G4int                               fWithBoratedPET = 0;
  G4int				      fWithGdWater = 0;
  G4Material*                         CombinedArXeHe3;
  G4Material*			      water;	
  void                                SetXeConc(G4double nf);
  void                                SetHe3Conc(G4double nf);
  void 				      SetOuterCryostatRadius(G4double rad);
  void 				      SetCryostatHeight(G4double height);
};

#endif
