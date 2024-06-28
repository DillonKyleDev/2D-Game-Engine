#include "BlobParticle.h"
#include "Animation.h"
#include "FlatEngine.h"

BlobParticle::BlobParticle(long ownerID) : GameScript(ownerID)
{
	SetName("BlobParticle");
}

BlobParticle::~BlobParticle()
{
}

void BlobParticle::Start()
{
	GetOwner()->GetAnimationComponent()->AddEventFunction("DestroySelf", FlatEngine::DestroySelf);
}

void BlobParticle::Update(float deltaTime)
{
}