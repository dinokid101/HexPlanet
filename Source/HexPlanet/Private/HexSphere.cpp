// Fill out your copyright notice in the Description page of Project Settings.

#include "HexPlanet.h"
#include "HexSphere.h"
#include "GridGenerator.h"
#include "GridTileComponent.h"

// Sets default values
AHexSphere::AHexSphere() 
{
	USceneComponent* SphereComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent = SphereComponent;	
	gridRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GridRoot"));
	gridRoot->AttachTo(RootComponent);
	pentagonMeshComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PentagonStaticMesh"));
	pentagonMeshComponent->AttachTo(gridRoot);
	hexagonMeshComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HexagonStaticMesh"));
	hexagonMeshComponent->AttachTo(gridRoot);
	radius = 50;
	numSubvisions = 0;
	gridGenerator = new GridGenerator(numSubvisions);
	surfaceArea = gridGenerator->getSurfaceArea(radius);
	volume = gridGenerator->getVolume(radius);
	GridTilePtrList tiles = gridGenerator->getTiles();
	numTiles = tiles.Num();
	PentagonMeshInnerRadius = 20;
	HexagonMeshInnerRadius = 20;
	tileFillRatio = 0.95;
	GridTiles.Empty();
#ifdef WITH_EDITOR
	debugMesh = CreateDefaultSubobject<ULineBatchComponent>(TEXT("DebugMeshRoot"));
	subdivisionPreviewMesh = CreateDefaultSubobject<ULineBatchComponent>(TEXT("SubdivisionPreviewGenerator"));
	debugMesh->AttachTo(RootComponent);
	subdivisionPreviewMesh->AttachTo(RootComponent);
	renderNodes = false;
	renderEdges = true;
	displayEdgeLengths = false;
	displayTileMeshes = true;
	displayCollisionTileMeshes = false;
	previewNextSubdivision = false;
#endif // WITH_EDITOR

 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AHexSphere::BeginPlay()
{
	Super::BeginPlay();
	numSubvisions = 0;
	calculateMesh();
}

// Called every frame
void AHexSphere::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );
	if (numSubvisions < 6)
	{
		numSubvisions++;
		calculateMesh();
	}
}

void AHexSphere::Destroyed()
{
	delete gridGenerator;
	gridGenerator = nullptr;
	AActor::Destroyed();
}

void AHexSphere::PostLoadSubobjects( FObjectInstancingGraph* OuterInstanceGraph)
{
	Super::PostLoadSubobjects(OuterInstanceGraph);
	calculateMesh();
}

TArray<UGridTileComponent*> AHexSphere::GetGridTiles() const
{
	return GridTiles;
}

UGridTileComponent* AHexSphere::GetGridTile(const int32& tileKey) const
{
	if (GridTiles.Num() < tileKey)
	{
		return GridTiles[tileKey];
	}
	return nullptr;
}

#if WITH_EDITOR  
void AHexSphere::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	//Get all of our components  
	TArray<UActorComponent*> OwnedComponents;
	GetComponents(OwnedComponents);

	//Get the name of the property that was changed  
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// We test using GET_MEMBER_NAME_CHECKED so that if someone changes the property name  
	// in the future this will fail to compile and we can update it.  
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, numSubvisions)))
	{
		calculateMesh();
	}
	else if ((PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, radius))
		|| (PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, HexagonMeshInnerRadius))
		|| (PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, PentagonMeshInnerRadius))
		||(PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, PentagonMesh))
		|| (PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, HexagonMesh))
		|| (PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, tileFillRatio)))
	{
		rebuildInstances();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, displayTileMeshes))
	{
		hexagonMeshComponent->SetVisibility(displayTileMeshes);
		pentagonMeshComponent->SetVisibility(displayTileMeshes);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, displayCollisionTileMeshes))
	{
		for (auto& pointerPair : GridTiles)
		{
			pointerPair->SetVisibility(displayCollisionTileMeshes);
		}
	}
	else if ((PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, renderNodes))
		 ||(PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, renderEdges))
		|| (PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, renderTileCenters))
		|| (PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, renderEdgeCenters)))
	{
		rebuildDebugMesh();
	}
	else if ((PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, displayEdgeLengths))
		|| (PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, displayNodeNames)))
	{
		updateDebugText();
	}
	else if ((PropertyName == GET_MEMBER_NAME_CHECKED(AHexSphere, previewNextSubdivision)))
	{
		genSubdivisionPreview();
	}
	
	

	// Call the base class version  
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void AHexSphere::OnConstruction(const FTransform& Transform)
{
	rebuildDebugMesh();
	genSubdivisionPreview();
}

#endif  

void AHexSphere::calculateMesh()
{
	//clean up the old grid
	pentagonMeshComponent->ClearInstances();
	hexagonMeshComponent->ClearInstances();
	gridGenerator->rebuildGrid(numSubvisions);
	surfaceArea = gridGenerator->getSurfaceArea(radius);
	volume = gridGenerator->getVolume(radius);
	GridTilePtrList tiles = gridGenerator->getTiles();
	numTiles = tiles.Num();
	rebuildInstances();
}

void AHexSphere::rebuildInstances()
{
	pentagonMeshComponent->ClearInstances();
	hexagonMeshComponent->ClearInstances();
	pentagonMeshComponent->SetStaticMesh(PentagonMesh);
	hexagonMeshComponent->SetStaticMesh(HexagonMesh);
	GridTilePtrList tiles = gridGenerator->getTiles();
	if (GridTiles.Num() < tiles.Num())
	{
#if WITH_EDITOR
		
#endif
		while (GridTiles.Num() < tiles.Num())
		{
			UGridTileComponent* newTile = NewObject<UGridTileComponent>(this);
			int32 tileKey = GridTiles.Add(newTile);
#if WITH_EDITOR
			USceneComponent* tileBucket = nullptr;
			int32 bucketKey = tileKey / 250;
			if (bucketKey >= GridTileBuckets.Num())
			{
				tileBucket = NewObject<USceneComponent>(this);
				bucketKey = GridTileBuckets.Add(tileBucket);
				tileBucket->AttachTo(gridRoot);
				tileBucket->RegisterComponent();
			}
			tileBucket = GridTileBuckets[bucketKey];
			newTile->AttachTo(tileBucket);
#else
			newTile->AttachTo(gridRoot);
#endif
			newTile->RegisterComponent();
			newTile->gridOwner = this;
			newTile->tileKey = tileKey;
			++tileKey;
		}
	}
	else
	{
		for (int32 oldIndex = GridTiles.Num(); oldIndex < GridTiles.Num(); ++oldIndex)
		{
			GridTiles[oldIndex]->DetachFromParent();
			GridTiles[oldIndex]->DestroyComponent();
		}
		GridTiles.SetNum(GridTiles.Num());
	}

	for (const GridTilePtr& tile : tiles)
	{		
		GridEdgePtrList tileEdges = tile->getEdges();
		if (tileEdges.Num() == 5 && PentagonMesh == nullptr)
		{
			continue;
		}
		else if (HexagonMesh == nullptr && tileEdges.Num()==6)
		{
			continue;
		}
		FVector tileCenter = tile->getPosition(radius);
		FVector zDir;
		float localSphereRadius;
		tileCenter.ToDirectionAndLength(zDir, localSphereRadius);
		FVector tileInnerRadiusVec = tileEdges[0]->getPosition(radius);
		tileInnerRadiusVec -= tileInnerRadiusVec.ProjectOnTo(zDir);
		float tileInnerRadius = FMath::Sqrt(FVector::DotProduct(tileInnerRadiusVec, tileInnerRadiusVec));
		FVector yVec;
		GridTilePtrList gridNeighbors = tile->getNeighbors();
		if (tileEdges.Num() == 5)
		{
			yVec = tileInnerRadiusVec;
		}
		else
		{
			GridTilePtr refNeighbor = gridNeighbors[0];
			FVector refenceVec = refNeighbor->getPosition(radius);
			gridNeighbors.Sort([&](const GridTile& neighbor1, const GridTile& neighbor2)->bool
			{
				return FVector::DistSquared(refenceVec, neighbor1.getPosition(radius)) > FVector::DistSquared(refenceVec, neighbor2.getPosition(radius));
			});
			GridTilePtr oppositeNeighbor = gridNeighbors[0];
			yVec = refenceVec - oppositeNeighbor->getPosition(radius);
		}
		yVec -= yVec.ProjectOnTo(zDir);
		float yVecMag;
		FVector yDir;
		yVec.ToDirectionAndLength(yDir, yVecMag);
		FVector xVec = FVector::CrossProduct(yDir, zDir);
		FVector xDir;
		float tangetRad;
		xVec.ToDirectionAndLength(xDir, tangetRad);
		
		UStaticMesh* myMesh = nullptr;
		int32 instanceMeshNum = 0;
		UInstancedStaticMeshComponent* tileMapMesher;
		FVector scaleVector = FVector(tileInnerRadius* tileFillRatio, tileInnerRadius* tileFillRatio, tileInnerRadius);
		if (tileEdges.Num() == 5)
		{
			scaleVector /= PentagonMeshInnerRadius;
			tileMapMesher = pentagonMeshComponent;
			myMesh = PentagonMesh;
		}
		else
		{
			scaleVector /= HexagonMeshInnerRadius;
			tileMapMesher = hexagonMeshComponent;
			myMesh = HexagonMesh;
		}

		UGridTileComponent* thisTile;
		int32 tileKey = tile->getIndex();

		thisTile = GridTiles[tileKey];
		thisTile->SetVisibility(displayCollisionTileMeshes);
		for (const GridTilePtr gridNeighborPtr : gridNeighbors)
		{
			thisTile->gridNeighborKeys.Add(gridNeighborPtr->getIndex());
		}

		FBox meshBB = myMesh->GetBoundingBox();
		FTransform instanceTransform(xDir, yDir, zDir, tileCenter);
		instanceTransform.SetScale3D(scaleVector);
		int32 instanceNum = tileMapMesher->AddInstance(instanceTransform);
		thisTile->mapMesh = tileMapMesher;
		thisTile->instanceMeshNum = instanceNum;
		instanceTransform.SetLocation(tileCenter+ 1.5*meshBB.Max.Z*zDir);
		thisTile->SetRelativeTransform(instanceTransform);
		thisTile->SetStaticMesh(myMesh);
	}


#ifdef WITH_EDITOR
	rebuildDebugMesh();
	updateDebugText();
	genSubdivisionPreview();
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void AHexSphere::buildDebugMesh()
{
	GridNodePtrList gridNodes = gridGenerator->getNodes();
	FVector centerPoint = GetActorLocation();
	if (renderNodes)
	{
		for (const GridNodePtr& gridNode : gridNodes)
		{
			debugMesh->DrawPoint(centerPoint + gridNode->getPosition(radius), FLinearColor::Blue, 8, 2);
		}
	}

	if (renderEdges|| renderEdgeCenters)
	{
		GridEdgePtrList gridEdges = gridGenerator->getEdges();
		for (const GridEdgePtr& gridEdge : gridEdges)
		{
			if (renderEdges)
			{
				debugMesh->DrawLine(centerPoint+gridEdge->getStartPoint()->getPosition(radius),
							centerPoint + gridEdge->getEndPoint()->getPosition(radius), FLinearColor::Green, 2, 0.5);
			}
			if (renderEdgeCenters)
			{
				debugMesh->DrawPoint(centerPoint + gridEdge->getPosition(radius), FLinearColor::Red, 8, 2);
			}
		}
	}

	if (renderTileCenters)
	{
		GridTilePtrList gridTiles = gridGenerator->getTiles();
		for (const GridTilePtr& gridTile : gridTiles)
		{
			debugMesh->DrawPoint(centerPoint + gridTile->getPosition(radius), FLinearColor::Yellow, 8, 2);
		}
	}
}

void AHexSphere::rebuildDebugMesh()
{
	debugMesh->Flush();

	if (renderNodes || renderEdges || renderTileCenters || renderEdgeCenters)
	{
		buildDebugMesh();
	}
}

void AHexSphere::updateDebugText()
{
	for (USceneComponent* debugText : debugTextArray)
	{
		debugText->DetachFromParent();
		debugText->DestroyComponent();
	}
	if (displayEdgeLengths)
	{
		GridEdgePtrList gridEdges = gridGenerator->getEdges();
		for (const GridEdgePtr& gridEdge : gridEdges)
		{
			FString stringForText = FString::FromInt(gridEdge->getIndex()).Append(FString(": ")).Append(FString::SanitizeFloat(gridEdge->getLength(radius)));
			FText nodeText = FText::FromString(stringForText);
			UTextRenderComponent* nodePos = NewObject<UTextRenderComponent>(this);
			nodePos->RegisterComponent();
			debugTextArray.Add(nodePos);
			nodePos->SetText(nodeText);
			nodePos->SetRelativeLocation(gridEdge->getPosition(radius));
			nodePos->SetWorldSize(1);
			nodePos->SetTextRenderColor(FColor::Black);
			nodePos->AttachTo(RootComponent);
		}
	}
	if (displayNodeNames)
	{
		GridNodePtrList gridNodes = gridGenerator->getNodes();
		for (GridNodePtr gridNode : gridNodes)
		{
			FString stringForText = FString::FromInt(gridNode->getIndex()).Append(FString(": ")).Append(gridNode->getPosition().ToString());
			FText nodeText = FText::FromString(stringForText);
			UTextRenderComponent* nodePos = NewObject<UTextRenderComponent>(this);
			nodePos->RegisterComponent();
			debugTextArray.Add(nodePos);
			nodePos->SetText(nodeText);
			nodePos->SetRelativeLocation(gridNode->getPosition(radius));
			nodePos->SetWorldSize(1);
			nodePos->SetTextRenderColor(FColor::Black);
			nodePos->AttachTo(RootComponent);
		}

	}
	
}

void AHexSphere::genSubdivisionPreview()
{
	subdivisionPreviewMesh->Flush();
	if (previewNextSubdivision)
	{
		FVector centerPoint = GetActorLocation();
		GridTilePtr tile0 = gridGenerator->getTile(0);
		previewTileSubdivision(tile0->getIndex(), centerPoint);
		GridTilePtrList tile0Neighbors = tile0->getNeighbors();
		for (GridTilePtr neighbor : tile0Neighbors)
		{
			previewTileSubdivision(neighbor->getIndex(), centerPoint);
		}

	}
}

void AHexSphere::previewTileSubdivision(uint32 tileIndex, FVector centerPoint)
{
	GridTilePtr tile0 = gridGenerator->getTile(tileIndex);
	GridEdgePtrList tile0Edges = tile0->getEdges();
	for (const GridEdgePtr& tile0Edge : tile0Edges)
	{
		FVector spPos = tile0Edge->getStartPoint()->getPosition();
		FVector spPos3D = tile0Edge->getStartPoint()->getPosition(radius);
		FVector epPos = tile0Edge->getEndPoint()->getPosition();
		FVector epPos3D = tile0Edge->getEndPoint()->getPosition(radius);
		FVector t1Pos = tile0Edge->getTiles()[0]->getPosition();
		FVector t1Pos3D = tile0Edge->getTiles()[0]->getPosition(radius);
		FVector t2Pos = tile0Edge->getTiles()[1]->getPosition();
		FVector t2Pos3D = tile0Edge->getTiles()[1]->getPosition(radius);

		subdivisionPreviewMesh->DrawLine(centerPoint + spPos3D, centerPoint + epPos3D, FLinearColor::Green, 2, 1);
		FVector newSp = gridGenerator->findAveragePoint(spPos, epPos, t1Pos);
		FVector newSp3D = newSp*radius;
		FVector newEp = gridGenerator->findAveragePoint(spPos, epPos, t2Pos);
		FVector newEp3D = newEp*radius;
		subdivisionPreviewMesh->DrawLine(centerPoint + newSp3D, centerPoint + newEp3D, FLinearColor::Red, 2, 1);

		FVector spT1 = gridGenerator->findAveragePoint(spPos, t1Pos);
		FVector spT13D = spT1*radius;
		subdivisionPreviewMesh->DrawLine(centerPoint + newSp3D, centerPoint + spT13D, FLinearColor::Yellow, 2, 1);
		FVector epT1 = gridGenerator->findAveragePoint(epPos, t1Pos);
		FVector epT13D = epT1*radius;
		subdivisionPreviewMesh->DrawLine(centerPoint + newSp3D, centerPoint + epT13D, FLinearColor::Yellow, 2, 1);
		FVector spT2 = gridGenerator->findAveragePoint(spPos, t2Pos);
		FVector spT23D = spT2*radius;
		subdivisionPreviewMesh->DrawLine(centerPoint + newEp3D, centerPoint + spT23D, FLinearColor::Yellow, 2, 1);
		FVector epT2 = gridGenerator->findAveragePoint(epPos, t2Pos);
		FVector epT23D = epT2*radius;
		subdivisionPreviewMesh->DrawLine(centerPoint + newEp3D, centerPoint + epT23D, FLinearColor::Yellow, 2, 1);

	}
}

#endif



