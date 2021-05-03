#ifndef WLGDCrystalHit_h
#define WLGDCrystalHit_h 1

#include "G4VHit.hh"
#include "G4THitsCollection.hh"
#include "G4Allocator.hh"
#include "G4ThreeVector.hh"

/// Crystal hit class
///
/// It defines data members to store the energy deposit,
/// and position in a selected volume:

class WLGDCrystalHit : public G4VHit
{
  public:
    WLGDCrystalHit();
    WLGDCrystalHit(const WLGDCrystalHit&);
    virtual ~WLGDCrystalHit();

    // operators
    const WLGDCrystalHit& operator=(const WLGDCrystalHit&);
    G4bool operator==(const WLGDCrystalHit&) const;

    inline void* operator new(size_t);
    inline void  operator delete(void*);

    // methods from base class
    virtual void Draw();
    virtual void Print();

    // Set methods
    void SetTID      (G4int   tid)      { fTid    = tid; };
    void SetTime     (G4double ti)      { fTime   = ti; };
    void SetWeight   (G4double we)      { fWeight = we; };
    void SetEdep     (G4double de)      { fEdep   = de; };
    void SetPos      (G4ThreeVector xyz){ fPos    = xyz; };
    void SetWhichReentranceTube      (G4int n){ fwhichReentranceTuber    = n; };

    // Get methods
    G4int    GetTID()  const     { return fTid; };
    G4double GetTime() const     { return fTime; };
    G4double GetWeight() const   { return fWeight; };
    G4double GetEdep() const     { return fEdep; };
    G4ThreeVector GetPos() const { return fPos; };
    G4int GetWhichReentranceTube() const { return fwhichReentranceTuber; };

  private:

      G4int         fTid;
      G4double      fTime;
      G4double      fWeight;
      G4double      fEdep;
      G4ThreeVector fPos;
      G4int         fwhichReentranceTuber;
};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

typedef G4THitsCollection<WLGDCrystalHit> WLGDCrystalHitsCollection;

extern G4ThreadLocal G4Allocator<WLGDCrystalHit>* WLGDCrystalHitAllocator;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

inline void* WLGDCrystalHit::operator new(size_t)
{
  if(!WLGDCrystalHitAllocator)
      WLGDCrystalHitAllocator = new G4Allocator<WLGDCrystalHit>;
  return (void *) WLGDCrystalHitAllocator->MallocSingle();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

inline void WLGDCrystalHit::operator delete(void *hit)
{
  WLGDCrystalHitAllocator->FreeSingle((WLGDCrystalHit*) hit);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif

