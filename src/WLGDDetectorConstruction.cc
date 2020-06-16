#include "WLGDDetectorConstruction.hh"

#include <set>

#include "G4RunManager.hh"

#include "G4Box.hh"
#include "G4Cons.hh"
#include "G4GeometryManager.hh"
#include "G4LogicalVolume.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4SolidStore.hh"
#include "G4Tubs.hh"

#include "G4Colour.hh"
#include "G4VisAttributes.hh"

#include "G4MultiFunctionalDetector.hh"
#include "G4SDManager.hh"
#include "G4SDParticleFilter.hh"
#include "G4VPrimitiveScorer.hh"
#include "WLGDPSEnergyDeposit.hh"
#include "WLGDPSLocation.hh"
#include "WLGDPSParentID.hh"
#include "WLGDPSTime.hh"
#include "WLGDPSTrackID.hh"

#include "WLGDBiasMultiParticleChangeCrossSection.hh"

#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"

WLGDDetectorConstruction::WLGDDetectorConstruction()
{
  DefineCommands();

  DefineMaterials();
}

WLGDDetectorConstruction::~WLGDDetectorConstruction()
{
  delete fDetectorMessenger;
  delete fBiasMessenger;
}

auto WLGDDetectorConstruction::Construct() -> G4VPhysicalVolume*
{
  // Cleanup old geometry
  G4GeometryManager::GetInstance()->OpenGeometry();
  G4PhysicalVolumeStore::GetInstance()->Clean();
  G4LogicalVolumeStore::GetInstance()->Clean();
  G4SolidStore::GetInstance()->Clean();

  if(fGeometryName == "baseline")
  {
    return SetupBaseline();
  }

  return SetupAlternative();
}

void WLGDDetectorConstruction::DefineMaterials()
{
  G4NistManager* nistManager = G4NistManager::Instance();
  nistManager->FindOrBuildMaterial("G4_Galactic");
  nistManager->FindOrBuildMaterial("G4_lAr");
  nistManager->FindOrBuildMaterial("G4_AIR");
  nistManager->FindOrBuildMaterial("G4_STAINLESS-STEEL");
  nistManager->FindOrBuildMaterial("G4_Cu");
  nistManager->FindOrBuildMaterial("G4_WATER");

  auto* C  = new G4Element("Carbon", "C", 6., 12.011 * g / mole);
  auto* O  = new G4Element("Oxygen", "O", 8., 16.00 * g / mole);
  auto* Ca = new G4Element("Calcium", "Ca", 20., 40.08 * g / mole);
  auto* Mg = new G4Element("Magnesium", "Mg", 12., 24.31 * g / mole);

  // Standard Rock definition, similar to Gran Sasso rock
  // with density from PDG report
  auto* stdRock = new G4Material("StdRock", 2.65 * g / cm3, 4);
  stdRock->AddElement(O, 52.0 * perCent);
  stdRock->AddElement(Ca, 27.0 * perCent);
  stdRock->AddElement(C, 12.0 * perCent);
  stdRock->AddElement(Mg, 9.0 * perCent);

  auto* H     = new G4Element("Hydrogen", "H", 1., 1.00794 * g / mole);
  auto* N     = new G4Element("Nitrogen", "N", 7., 14.00 * g / mole);
  auto* puMat = new G4Material("polyurethane", 0.3 * g / cm3, 4);  // high density foam
  puMat->AddElement(H, 16);
  puMat->AddElement(O, 2);
  puMat->AddElement(C, 8);
  puMat->AddElement(N, 2);

  // enriched Germanium from isotopes
  auto* Ge_74 = new G4Isotope("Ge74", 32, 74, 74.0 * g / mole);
  auto* Ge_76 = new G4Isotope("Ge76", 32, 76, 76.0 * g / mole);

  auto* eGe = new G4Element("enriched Germanium", "enrGe", 2);
  eGe->AddIsotope(Ge_76, 88. * perCent);
  eGe->AddIsotope(Ge_74, 12. * perCent);

  G4double density = 3.323 * mg / cm3;
  auto*    roiMat  = new G4Material("enrGe", density, 1);
  roiMat->AddElement(eGe, 1);
}

void WLGDDetectorConstruction::ConstructSDandField()
{
  G4SDManager::GetSDMpointer()->SetVerboseLevel(2);

  // Only need to construct the (per-thread) SD once
  if(!fSD.Get())
  {
    auto* det = new G4MultiFunctionalDetector("Det");
    fSD.Put(det);

    auto* vertexFilter = new G4SDParticleFilter("vtxfilt");
    vertexFilter->add("neutron");  // neutrons in Ge of interest
    vertexFilter->addIon(32, 77);  // register 77Ge production

    auto* eprimitive = new WLGDPSEnergyDeposit("Edep");
    eprimitive->SetFilter(vertexFilter);
    det->RegisterPrimitive(eprimitive);

    auto* tprimitive = new WLGDPSTime("Time");
    tprimitive->SetFilter(vertexFilter);
    det->RegisterPrimitive(tprimitive);

    auto* lprimitive = new WLGDPSLocation("Loc");
    lprimitive->SetFilter(vertexFilter);
    det->RegisterPrimitive(lprimitive);

    auto* idprimitive = new WLGDPSTrackID("TrackID");
    idprimitive->SetFilter(vertexFilter);
    det->RegisterPrimitive(idprimitive);

    auto* primitive = new WLGDPSParentID("ParentID");
    primitive->SetFilter(vertexFilter);
    det->RegisterPrimitive(primitive);

    // Also only add it once to the SD manager!
    G4SDManager::GetSDMpointer()->AddNewDetector(fSD.Get());

    SetSensitiveDetector("Ge_log", fSD.Get());

    // ----------------------------------------------
    // -- operator creation and attachment to volume:
    // ----------------------------------------------
    G4LogicalVolumeStore* volumeStore = G4LogicalVolumeStore::GetInstance();

    // -- Attach neutron XS biasing to Germanium -> enhance nCapture
    auto* biasnXS = new WLGDBiasMultiParticleChangeCrossSection();
    biasnXS->SetNeutronFactor(fNeutronBias);
    biasnXS->SetMuonFactor(fMuonBias);
    G4cout << " >>> Detector: set neutron bias to " << fNeutronBias << G4endl;
    biasnXS->AddParticle("neutron");
    G4LogicalVolume* logicGe = volumeStore->GetVolume("Ge_log");
    biasnXS->AttachTo(logicGe);

    // -- Attach muon XS biasing to all required volumes consistently
    auto* biasmuXS = new WLGDBiasMultiParticleChangeCrossSection();
    biasmuXS->SetNeutronFactor(fNeutronBias);
    biasmuXS->SetMuonFactor(fMuonBias);
    G4cout << " >>> Detector: set muon bias to " << fMuonBias << G4endl;
    biasmuXS->AddParticle("mu-");

    G4LogicalVolume* logicCavern = volumeStore->GetVolume("Cavern_log");
    biasmuXS->AttachTo(logicCavern);
    G4LogicalVolume* logicHall = volumeStore->GetVolume("Hall_log");
    biasmuXS->AttachTo(logicHall);
    G4LogicalVolume* logicTank = volumeStore->GetVolume("Tank_log");
    biasmuXS->AttachTo(logicTank);
    G4LogicalVolume* logicLar = volumeStore->GetVolume("Lar_log");
    biasmuXS->AttachTo(logicLar);
    G4LogicalVolume* logicCu = volumeStore->GetVolume("Copper_log");
    biasmuXS->AttachTo(logicCu);
    G4LogicalVolume* logicULar = volumeStore->GetVolume("ULar_log");
    biasmuXS->AttachTo(logicULar);

    // Baseline also has a water volume and cryostat
    if(fGeometryName == "baseline")
    {
      G4LogicalVolume* logicWater = volumeStore->GetVolume("Water_log");
      biasmuXS->AttachTo(logicWater);
      G4LogicalVolume* logicCout = volumeStore->GetVolume("Cout_log");
      biasmuXS->AttachTo(logicCout);
      G4LogicalVolume* logicCinn = volumeStore->GetVolume("Cinn_log");
      biasmuXS->AttachTo(logicCinn);
      G4LogicalVolume* logicCLid = volumeStore->GetVolume("Lid_log");
      biasmuXS->AttachTo(logicCLid);
      G4LogicalVolume* logicCBot = volumeStore->GetVolume("Bot_log");
      biasmuXS->AttachTo(logicCBot);
    }
    // Alternative has the membrane and insulator
    else if(fGeometryName == "alternative")
    {
      G4LogicalVolume* logicPu = volumeStore->GetVolume("Pu_log");
      biasmuXS->AttachTo(logicPu);
      G4LogicalVolume* logicMembrane = volumeStore->GetVolume("Membrane_log");
      biasmuXS->AttachTo(logicMembrane);
    }
  }
  else
  {
    G4cout << " >>> fSD has entry. Repeated call." << G4endl;
  }
}

auto WLGDDetectorConstruction::SetupAlternative() -> G4VPhysicalVolume*
{
  // Get materials
  auto* worldMaterial = G4Material::GetMaterial("G4_Galactic");
  auto* larMat        = G4Material::GetMaterial("G4_lAr");
  auto* airMat        = G4Material::GetMaterial("G4_AIR");
  auto* steelMat      = G4Material::GetMaterial("G4_STAINLESS-STEEL");
  auto* copperMat     = G4Material::GetMaterial("G4_Cu");
  auto* stdRock       = G4Material::GetMaterial("StdRock");
  auto* puMat         = G4Material::GetMaterial("polyurethane");
  auto* roiMat        = G4Material::GetMaterial("enrGe");

  // size parameter, unit [cm]
  // cavern
  G4double stone     = 100.0;  // Hall wall thickness 1 m
  G4double hallhside = 850.0;  // Hall cube side 17 m
  // cryostat
  G4double tankhside  = 650;   // cryostat cube side 13 m
  G4double outerwall  = 1.2;   // outer SS wall thickness
  G4double insulation = 80.0;  // polyurethane foam
  G4double innerwall  = 0.12;  // inner SS membrane
  // copper tubes with Germanium ROI
  G4double copper    = 0.35;    // tube thickness 0.5 mm
  G4double curad     = 40.0;    // copper tube diam 80 cm
  G4double cuhheight = 334.34;  // copper tube height 7 m inside cryostat
  G4double cushift   = 234.34;  // shift cu tube inside cryostat to top
  G4double ringrad   = 100.0;   // cu tube placement ring radius
  // Ge cylinder for 250 kg at 5.32 g/cm3
  G4double roiradius     = 25.0;   // detector region diam 50 cm
  G4double roihalfheight = 11.97;  // detector region height 24 cm

  // total
  G4double offset =
    hallhside - tankhside;  // shift cavern floor to keep detector centre at origin
  G4double worldside = hallhside + stone + offset + 0.1;  // larger than rest
  G4double larside =
    tankhside - outerwall - insulation - innerwall;  // cube side of LAr volume

  fvertexZ = (worldside - 0.1) * cm;  // max vertex height
  fmaxrad  = fvertexZ - stone * cm;   // max vertex circle radius

  // Volumes for this geometry

  //
  // World
  //
  auto* worldSolid = new G4Box("World", worldside * cm, worldside * cm, worldside * cm);
  auto* fWorldLogical  = new G4LogicalVolume(worldSolid, worldMaterial, "World_log");
  auto* fWorldPhysical = new G4PVPlacement(nullptr, G4ThreeVector(), fWorldLogical,
                                           "World_phys", nullptr, false, 0);

  //
  // Cavern
  //
  auto* cavernSolid    = new G4Box("Cavern", (hallhside + stone) * cm,
                                (hallhside + stone) * cm, (hallhside + stone) * cm);
  auto* fCavernLogical = new G4LogicalVolume(cavernSolid, stdRock, "Cavern_log");
  auto* fCavernPhysical =
    new G4PVPlacement(nullptr, G4ThreeVector(0., 0., offset * cm), fCavernLogical,
                      "Cavern_phys", fWorldLogical, false, 0);

  //
  // Hall
  //
  auto* hallSolid = new G4Box("Cavern", hallhside * cm, hallhside * cm, hallhside * cm);
  auto* fHallLogical = new G4LogicalVolume(hallSolid, airMat, "Hall_log");
  auto* fHallPhysical =
    new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -stone * cm), fHallLogical,
                      "Hall_phys", fCavernLogical, false, 0, true);

  //
  // Tank
  //
  auto* tankSolid    = new G4Box("Tank", tankhside * cm, tankhside * cm, tankhside * cm);
  auto* fTankLogical = new G4LogicalVolume(tankSolid, steelMat, "Tank_log");
  auto* fTankPhysical =
    new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -stone * cm), fTankLogical,
                      "Tank_phys", fHallLogical, false, 0, true);

  //
  // Insulator
  //
  auto* puSolid     = new G4Box("Insulator", (tankhside - outerwall) * cm,
                            (tankhside - outerwall) * cm, (tankhside - outerwall) * cm);
  auto* fPuLogical  = new G4LogicalVolume(puSolid, puMat, "Pu_log");
  auto* fPuPhysical = new G4PVPlacement(nullptr, G4ThreeVector(), fPuLogical, "Pu_phys",
                                        fTankLogical, false, 0, true);

  //
  // Membrane
  //
  auto* membraneSolid = new G4Box("Membrane", (tankhside - outerwall - insulation) * cm,
                                  (tankhside - outerwall - insulation) * cm,
                                  (tankhside - outerwall - insulation) * cm);
  auto* fMembraneLogical = new G4LogicalVolume(membraneSolid, steelMat, "Membrane_log");
  auto* fMembranePhysical =
    new G4PVPlacement(nullptr, G4ThreeVector(), fMembraneLogical, "Membrane_phys",
                      fPuLogical, false, 0, true);

  //
  // LAr
  //
  auto* larSolid     = new G4Box("LAr", larside * cm, larside * cm, larside * cm);
  auto* fLarLogical  = new G4LogicalVolume(larSolid, larMat, "Lar_log");
  auto* fLarPhysical = new G4PVPlacement(nullptr, G4ThreeVector(), fLarLogical,
                                         "Lar_phys", fMembraneLogical, false, 0, true);

  //
  // copper tubes, hollow cylinder shell
  //
  auto* copperSolid = new G4Tubs("Copper", (curad - copper) * cm, curad * cm,
                                 cuhheight * cm, 0.0, CLHEP::twopi);

  //
  // ULAr bath, solid cylinder
  //
  auto* ularSolid = new G4Tubs("ULar", 0.0 * cm, (curad - copper) * cm, cuhheight * cm,
                               0.0, CLHEP::twopi);

  //
  // Germanium, solid cylinder
  //
  auto* geSolid =
    new G4Tubs("ROI", 0.0 * cm, roiradius * cm, roihalfheight * cm, 0.0, CLHEP::twopi);

  // tower; logical volumes
  auto* fCopperLogical = new G4LogicalVolume(copperSolid, copperMat, "Copper_log");
  auto* fUlarLogical   = new G4LogicalVolume(ularSolid, larMat, "ULar_log");
  auto* fGeLogical     = new G4LogicalVolume(geSolid, roiMat, "Ge_log");

  // placements
  new G4PVPlacement(nullptr, G4ThreeVector(ringrad * cm, 0., cushift * cm),
                    fCopperLogical, "Copper_phys", fLarLogical, false, 0, true);

  new G4PVPlacement(nullptr, G4ThreeVector(ringrad * cm, 0., cushift * cm), fUlarLogical,
                    "ULar_phys", fLarLogical, false, 0, true);

  new G4PVPlacement(nullptr, G4ThreeVector(0. * cm, 0. * cm, -cushift * cm), fGeLogical,
                    "Ge_phys", fUlarLogical, false, 0, true);

  // tower 2
  new G4PVPlacement(nullptr, G4ThreeVector(0., ringrad * cm, cushift * cm),
                    fCopperLogical, "Copper_phys2", fLarLogical, false, 1, true);

  new G4PVPlacement(nullptr, G4ThreeVector(0., ringrad * cm, cushift * cm), fUlarLogical,
                    "ULar_phys2", fLarLogical, false, 1, true);

  // tower 3
  new G4PVPlacement(nullptr, G4ThreeVector(-ringrad * cm, 0., cushift * cm),
                    fCopperLogical, "Copper_phys3", fLarLogical, false, 2, true);

  new G4PVPlacement(nullptr, G4ThreeVector(-ringrad * cm, 0., cushift * cm), fUlarLogical,
                    "ULar_phys3", fLarLogical, false, 2, true);

  // tower 4
  new G4PVPlacement(nullptr, G4ThreeVector(0., -ringrad * cm, cushift * cm),
                    fCopperLogical, "Copper_phys4", fLarLogical, false, 3, true);

  new G4PVPlacement(nullptr, G4ThreeVector(0., -ringrad * cm, cushift * cm), fUlarLogical,
                    "ULar_phys4", fLarLogical, false, 3, true);

  //
  // Visualization attributes
  //
  fWorldLogical->SetVisAttributes(G4VisAttributes::GetInvisible());

  auto* redVisAtt = new G4VisAttributes(G4Colour::Red());
  redVisAtt->SetVisibility(true);
  auto* greyVisAtt = new G4VisAttributes(G4Colour::Grey());
  greyVisAtt->SetVisibility(true);
  auto* greenVisAtt = new G4VisAttributes(G4Colour::Green());
  greenVisAtt->SetVisibility(true);
  auto* blueVisAtt = new G4VisAttributes(G4Colour::Blue());
  blueVisAtt->SetVisibility(true);

  fCavernLogical->SetVisAttributes(redVisAtt);
  fHallLogical->SetVisAttributes(greyVisAtt);
  fTankLogical->SetVisAttributes(blueVisAtt);
  fPuLogical->SetVisAttributes(greyVisAtt);
  fMembraneLogical->SetVisAttributes(blueVisAtt);
  fLarLogical->SetVisAttributes(greyVisAtt);
  fCopperLogical->SetVisAttributes(greenVisAtt);
  fUlarLogical->SetVisAttributes(greyVisAtt);
  fGeLogical->SetVisAttributes(redVisAtt);

  return fWorldPhysical;
}

auto WLGDDetectorConstruction::SetupBaseline() -> G4VPhysicalVolume*
{
  // Get materials
  auto* worldMaterial = G4Material::GetMaterial("G4_Galactic");
  auto* larMat        = G4Material::GetMaterial("G4_lAr");
  auto* airMat        = G4Material::GetMaterial("G4_AIR");
  auto* waterMat      = G4Material::GetMaterial("G4_WATER");
  auto* steelMat      = G4Material::GetMaterial("G4_STAINLESS-STEEL");
  auto* copperMat     = G4Material::GetMaterial("G4_Cu");
  auto* stdRock       = G4Material::GetMaterial("StdRock");
  auto* roiMat        = G4Material::GetMaterial("enrGe");

  // constants
  // size parameter, unit [cm]
  G4double offset = 200.0;  // shift cavern floor to keep detector centre at origin
  // cavern
  G4double stone       = 100.0;  // Hall wall thickness 1 m
  G4double hallrad     = 600.0;  // Hall cylinder diam 12 m
  G4double hallhheight = 850.0;  // Hall cylinder height 17 m
  // water tank
  G4double tankwalltop = 0.6;  // water tank thickness at top 6 mm
  G4double tankwallbot = 0.8;  // water tank thickness at bottom 8 mm
  G4double tankrad     = 550;  // water tank diam 11 m
  G4double tankhheight = 650;  // water tank height 13 m
  // cryostat
  G4double cryowall   = 3.0;    // cryostat wall thickness from GERDA
  G4double vacgap     = 1.0;    // vacuum gap between walls
  G4double cryrad     = 350.0;  // cryostat diam 7 m
  G4double cryhheight = 350.0;  // cryostat height 7 m
  // copper tubes with Germanium ROI
  G4double copper    = 0.35;   // tube thickness 3.5 mm
  G4double curad     = 40.0;   // copper tube diam 80 cm
  G4double cuhheight = 200.0;  // copper tube height 4 m inside cryostat
  G4double cushift   = 150.0;  // shift cu tube inside cryostat to top
  G4double ringrad   = 100.0;  // cu tube placement ring radius
  // Ge cylinder for 250 kg at 5.32 g/cm3
  G4double roiradius     = 25.0;   // detector region diam 50 cm
  G4double roihalfheight = 11.97;  // detector region height 24 cm

  fvertexZ = (hallhheight + stone + offset) * cm;
  fmaxrad  = hallrad * cm;

  // Volumes for this geometry

  //
  // World
  //
  auto* worldSolid =
    new G4Tubs("World", 0.0 * cm, (hallrad + stone + 0.1) * cm,
               (hallhheight + stone + offset + 0.1) * cm, 0.0, CLHEP::twopi);
  auto* fWorldLogical  = new G4LogicalVolume(worldSolid, worldMaterial, "World_log");
  auto* fWorldPhysical = new G4PVPlacement(nullptr, G4ThreeVector(), fWorldLogical,
                                           "World_phys", nullptr, false, 0);

  //
  // Cavern
  //
  auto* cavernSolid    = new G4Tubs("Cavern", 0.0 * cm, (hallrad + stone) * cm,
                                 (hallhheight + stone) * cm, 0.0, CLHEP::twopi);
  auto* fCavernLogical = new G4LogicalVolume(cavernSolid, stdRock, "Cavern_log");
  auto* fCavernPhysical =
    new G4PVPlacement(nullptr, G4ThreeVector(0., 0., offset * cm), fCavernLogical,
                      "Cavern_phys", fWorldLogical, false, 0);

  //
  // Hall
  //
  auto* hallSolid =
    new G4Tubs("Hall", 0.0 * cm, hallrad * cm, hallhheight * cm, 0.0, CLHEP::twopi);
  auto* fHallLogical = new G4LogicalVolume(hallSolid, airMat, "Hall_log");
  auto* fHallPhysical =
    new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -stone * cm), fHallLogical,
                      "Hall_phys", fCavernLogical, false, 0, true);

  //
  // Tank
  //
  auto* tankSolid =
    new G4Cons("Tank", 0.0 * cm, (tankrad + tankwallbot) * cm, 0.0 * cm,
               (tankrad + tankwalltop) * cm, tankhheight * cm, 0.0, CLHEP::twopi);
  auto* fTankLogical = new G4LogicalVolume(tankSolid, steelMat, "Tank_log");
  auto* fTankPhysical =
    new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -stone * cm), fTankLogical,
                      "Tank_phys", fHallLogical, false, 0, true);

  //
  // Water
  //
  auto* waterSolid     = new G4Tubs("Water", 0.0 * cm, tankrad * cm,
                                (tankhheight - tankwallbot) * cm, 0.0, CLHEP::twopi);
  auto* fWaterLogical  = new G4LogicalVolume(waterSolid, waterMat, "Water_log");
  auto* fWaterPhysical = new G4PVPlacement(nullptr, G4ThreeVector(), fWaterLogical,
                                           "Water_phys", fTankLogical, false, 0, true);

  //
  // outer cryostat
  //
  auto* coutSolid =
    new G4Tubs("Cout", 0.0 * cm, cryrad * cm, cryhheight * cm, 0.0, CLHEP::twopi);
  auto* fCoutLogical  = new G4LogicalVolume(coutSolid, steelMat, "Cout_log");
  auto* fCoutPhysical = new G4PVPlacement(nullptr, G4ThreeVector(), fCoutLogical,
                                          "Cout_phys", fWaterLogical, false, 0, true);

  //
  // vacuum gap
  //
  auto* cvacSolid     = new G4Tubs("Cvac", 0.0 * cm, (cryrad - cryowall) * cm,
                               cryhheight * cm, 0.0, CLHEP::twopi);
  auto* fCvacLogical  = new G4LogicalVolume(cvacSolid, worldMaterial, "Cvac_log");
  auto* fCvacPhysical = new G4PVPlacement(nullptr, G4ThreeVector(), fCvacLogical,
                                          "Cvac_phys", fCoutLogical, false, 0, true);

  //
  // inner cryostat
  //
  auto* cinnSolid     = new G4Tubs("Cinn", 0.0 * cm, (cryrad - cryowall - vacgap) * cm,
                               cryhheight * cm, 0.0, CLHEP::twopi);
  auto* fCinnLogical  = new G4LogicalVolume(cinnSolid, steelMat, "Cinn_log");
  auto* fCinnPhysical = new G4PVPlacement(nullptr, G4ThreeVector(), fCinnLogical,
                                          "Cinn_phys", fCvacLogical, false, 0, true);

  //
  // LAr bath
  //
  auto* larSolid     = new G4Tubs("LAr", 0.0 * cm, (cryrad - 2 * cryowall - vacgap) * cm,
                              cryhheight * cm, 0.0, CLHEP::twopi);
  auto* fLarLogical  = new G4LogicalVolume(larSolid, larMat, "Lar_log");
  auto* fLarPhysical = new G4PVPlacement(nullptr, G4ThreeVector(), fLarLogical,
                                         "Lar_phys", fCinnLogical, false, 0, true);

  //
  // cryostat Lid
  //
  auto* lidSolid =
    new G4Tubs("Lid", 0.0 * cm, cryrad * cm, cryowall / 2.0 * cm, 0.0, CLHEP::twopi);
  auto* fLidLogical = new G4LogicalVolume(lidSolid, steelMat, "Lid_log");
  auto* fLidPhysical =
    new G4PVPlacement(nullptr, G4ThreeVector(0., 0., (cryhheight + cryowall / 2.0) * cm),
                      fLidLogical, "Lid_phys", fWaterLogical, false, 0, true);
  auto* fBotLogical = new G4LogicalVolume(lidSolid, steelMat, "Bot_log");
  auto* fBotPhysical =
    new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -(cryhheight + cryowall / 2.0) * cm),
                      fBotLogical, "Bot_phys", fWaterLogical, false, 0, true);

  //
  // copper tubes, hollow cylinder shell
  //
  auto* copperSolid = new G4Tubs("Copper", (curad - copper) * cm, curad * cm,
                                 cuhheight * cm, 0.0, CLHEP::twopi);

  //
  // ULAr bath, solid cylinder
  //
  auto* ularSolid = new G4Tubs("ULar", 0.0 * cm, (curad - copper) * cm, cuhheight * cm,
                               0.0, CLHEP::twopi);

  //
  // Germanium, solid cylinder
  //
  auto* geSolid =
    new G4Tubs("ROI", 0.0 * cm, roiradius * cm, roihalfheight * cm, 0.0, CLHEP::twopi);

  // tower; logical volumes
  auto* fCopperLogical = new G4LogicalVolume(copperSolid, copperMat, "Copper_log");
  auto* fUlarLogical   = new G4LogicalVolume(ularSolid, larMat, "ULar_log");
  auto* fGeLogical     = new G4LogicalVolume(geSolid, roiMat, "Ge_log");

  // placements
  new G4PVPlacement(nullptr, G4ThreeVector(ringrad * cm, 0., cushift * cm),
                    fCopperLogical, "Copper_phys", fLarLogical, false, 0, true);

  new G4PVPlacement(nullptr, G4ThreeVector(ringrad * cm, 0., cushift * cm), fUlarLogical,
                    "ULar_phys", fLarLogical, false, 0, true);

  new G4PVPlacement(nullptr, G4ThreeVector(0. * cm, 0. * cm, -cushift * cm), fGeLogical,
                    "Ge_phys", fUlarLogical, false, 0, true);

  // tower 2
  new G4PVPlacement(nullptr, G4ThreeVector(0., ringrad * cm, cushift * cm),
                    fCopperLogical, "Copper_phys2", fLarLogical, false, 1, true);

  new G4PVPlacement(nullptr, G4ThreeVector(0., ringrad * cm, cushift * cm), fUlarLogical,
                    "ULar_phys2", fLarLogical, false, 1, true);

  // tower 3
  new G4PVPlacement(nullptr, G4ThreeVector(-ringrad * cm, 0., cushift * cm),
                    fCopperLogical, "Copper_phys3", fLarLogical, false, 2, true);

  new G4PVPlacement(nullptr, G4ThreeVector(-ringrad * cm, 0., cushift * cm), fUlarLogical,
                    "ULar_phys3", fLarLogical, false, 2, true);

  // tower 4
  new G4PVPlacement(nullptr, G4ThreeVector(0., -ringrad * cm, cushift * cm),
                    fCopperLogical, "Copper_phys4", fLarLogical, false, 3, true);

  new G4PVPlacement(nullptr, G4ThreeVector(0., -ringrad * cm, cushift * cm), fUlarLogical,
                    "ULar_phys4", fLarLogical, false, 3, true);

  //
  // Visualization attributes
  //
  fWorldLogical->SetVisAttributes(G4VisAttributes::GetInvisible());

  auto* redVisAtt = new G4VisAttributes(G4Colour::Red());
  redVisAtt->SetVisibility(true);
  auto* greyVisAtt = new G4VisAttributes(G4Colour::Grey());
  greyVisAtt->SetVisibility(true);
  auto* greenVisAtt = new G4VisAttributes(G4Colour::Green());
  greenVisAtt->SetVisibility(true);
  auto* blueVisAtt = new G4VisAttributes(G4Colour::Blue());
  blueVisAtt->SetVisibility(true);

  fCavernLogical->SetVisAttributes(redVisAtt);
  fHallLogical->SetVisAttributes(greyVisAtt);
  fTankLogical->SetVisAttributes(greenVisAtt);
  fWaterLogical->SetVisAttributes(greyVisAtt);
  fLarLogical->SetVisAttributes(greyVisAtt);
  fCoutLogical->SetVisAttributes(blueVisAtt);
  fCvacLogical->SetVisAttributes(greyVisAtt);
  fCinnLogical->SetVisAttributes(blueVisAtt);
  fLidLogical->SetVisAttributes(blueVisAtt);
  fBotLogical->SetVisAttributes(blueVisAtt);
  fCopperLogical->SetVisAttributes(greenVisAtt);
  fUlarLogical->SetVisAttributes(greyVisAtt);
  fGeLogical->SetVisAttributes(redVisAtt);

  return fWorldPhysical;
}

void WLGDDetectorConstruction::SetGeometry(const G4String& name)
{
  std::set<G4String> knownGeometries = { "baseline", "alternative" };
  if(knownGeometries.count(name) == 0)
  {
    G4Exception("WLGDDetectorConstruction::SetGeometry", "WLGD0001", JustWarning,
                ("Invalid geometry setup name '" + name + "'").c_str());
    return;
  }

  fGeometryName = name;
  // Reinit wiping out stores
  G4RunManager::GetRunManager()->ReinitializeGeometry();
}

void WLGDDetectorConstruction::SetNeutronBiasFactor(G4double nf)
{
  fNeutronBias = nf;
}

void WLGDDetectorConstruction::SetMuonBiasFactor(G4double mf) { fMuonBias = mf; }

void WLGDDetectorConstruction::DefineCommands()
{
  // Define geometry command directory using generic messenger class
  fDetectorMessenger = new G4GenericMessenger(this, "/WLGD/detector/",
                                              "Commands for controlling detector setup");

  // switch command
  fDetectorMessenger->DeclareMethod("setGeometry", &WLGDDetectorConstruction::SetGeometry)
    .SetGuidance("Set geometry model of cavern and detector")
    .SetGuidance("baseline = NEEDS DESCRIPTION")
    .SetGuidance("alternative = NEEDS DESCRIPTION")
    .SetCandidates("baseline alternative")
    .SetStates(G4State_PreInit)
    .SetToBeBroadcasted(false);

  // Define bias operator command directory using generic messenger class
  fBiasMessenger =
    new G4GenericMessenger(this, "/WLGD/bias/", "Commands for controlling bias factors");

  // switch commands
  fBiasMessenger
    ->DeclareMethod("setNeutronBias", &WLGDDetectorConstruction::SetNeutronBiasFactor)
    .SetGuidance("Set Bias factor for neutron capture process.")
    .SetDefaultValue("1.0")
    .SetStates(G4State_PreInit)
    .SetToBeBroadcasted(false);
  fBiasMessenger
    ->DeclareMethod("setMuonBias", &WLGDDetectorConstruction::SetMuonBiasFactor)
    .SetGuidance("Set Bias factor for muon nuclear process.")
    .SetDefaultValue("1.0")
    .SetStates(G4State_PreInit)
    .SetToBeBroadcasted(false);
}