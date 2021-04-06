// WLGDPSLocation
#include "WLGDPSLocation.hh"
#include "G4UnitsTable.hh"
#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"

////////////////////////////////////////////////////////////////////////////////
// Description:
//   This is a primitive scorer class for scoring energy deposit location
//
///////////////////////////////////////////////////////////////////////////////

WLGDPSLocation::WLGDPSLocation(G4String name, G4int depth)
: G4VPrimitiveScorer(std::move(name), depth)
, HCID(-1)
, EvtMap(nullptr)
{
  SetUnit("m");
}

WLGDPSLocation::WLGDPSLocation(G4String name, const G4String& unit, G4int depth)
: G4VPrimitiveScorer(std::move(name), depth)
, HCID(-1)
, EvtMap(nullptr)
{
  SetUnit(unit);
}

WLGDPSLocation::~WLGDPSLocation() = default;

G4bool WLGDPSLocation::ProcessHits(G4Step* aStep, G4TouchableHistory* /*unused*/)
{
  if(aStep->GetTotalEnergyDeposit() <= 0.0)
    return false;

  G4int          index = GetIndex(aStep);
  G4TrackLogger& tlog  = fCellTrackLogger[index];
  if(tlog.FirstEnterance(aStep->GetTrack()->GetTrackID()))
  {
    G4cout << aStep->GetTrack()->GetParticleDefinition()->GetParticleName() << G4endl;
    G4StepPoint*  stepPoint = aStep->GetPreStepPoint();
    G4ThreeVector loc       = stepPoint->GetPosition();  // location at track creation

	G4cout << "Position of Ge-77: " << loc.getX()/m << " " << loc.getY()/m << " " << loc.getZ()/m << G4endl;
	G4cout << "Index of Step: " << index << G4endl;
    EvtMap->add(index, loc);
	G4cout << "EvtMap size while filling: " << EvtMap->size() << G4endl;
  }
  return true;
}

void WLGDPSLocation::Initialize(G4HCofThisEvent* HCE)
{
  EvtMap =
    new G4THitsMap<G4ThreeVector>(GetMultiFunctionalDetector()->GetName(), GetName());
  if(HCID < 0)
  {
    HCID = GetCollectionID(0);
  }
  HCE->AddHitsCollection(HCID, (G4VHitsCollection*) EvtMap);
}

void WLGDPSLocation::EndOfEvent(G4HCofThisEvent* /*unused*/) { fCellTrackLogger.clear(); }

void WLGDPSLocation::clear()
{
  fCellTrackLogger.clear();
  EvtMap->clear();
}

void WLGDPSLocation::DrawAll() { ; }

void WLGDPSLocation::PrintAll()
{
  G4cout << " MultiFunctionalDet  " << detector->GetName() << G4endl;
  G4cout << " PrimitiveScorer " << GetName() << G4endl;
  G4cout << " Number of entries " << EvtMap->entries() << G4endl;
  auto itr = EvtMap->GetMap()->begin();
  for(; itr != EvtMap->GetMap()->end(); itr++)
  {
    G4cout << "  key: " << itr->first << "  energy deposit at: ("
           << (*(itr->second)).x() / GetUnitValue() << ", "
           << (*(itr->second)).y() / GetUnitValue() << ", "
           << (*(itr->second)).z() / GetUnitValue() << ")"
           << " [" << GetUnit() << "]" << G4endl;
  }
}

void WLGDPSLocation::SetUnit(const G4String& unit) { CheckAndSetUnit(unit, "Length"); }
