// Fill out your copyright notice in the Description page of Project Settings.

#include "HexPlanet.h"
#include "GridMesher.h"




UGridMesher::UGridMesher()
{
	bWantsBeginPlay = false;
	PrimaryComponentTick.bCanEverTick = false;
	myGrid = nullptr;
	baseMeshRadius = 200;
	baseMeshMaterial = nullptr;
	numMeshes = 0;

	renderNodes = false;
	renderBaseMesh;
	debugLineOut = CreateDefaultSubobject<ULineBatchComponent>(TEXT("DebugLineDrawer"));
	//numberOfNodesToMesh = -1;
}


int32 UGridMesher::buildNewMesh(const TArray<float>& vertexRadii, const TArray<FColor>& vertexColors, UMaterialInterface* newMeshMaterial,
	int32 meshToRebuild /*= -1 if we're to build a new mesh*/)
{
	/*void CreateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices,
	const TArray<int32>& Triangles, const TArray<FVector>& Normals,
	const TArray<FVector2D>& UV0, const TArray<FColor>& VertexColors,
	const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision);*/
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FProcMeshTangent> Tangents;

	for (const FRectGridLocation& gridLoc : myGrid->gridLocationsM)
	{
		FVector tilePos = myGrid->getNodeLocationOnSphere(gridLoc);
		Vertices.Add(tilePos * vertexRadii[gridLoc.tileIndex]);
		FVector vertexNormal = calculateVertexNormal(gridLoc, vertexRadii);
		Normals.Add(vertexNormal);
	}

	for (int32 uLoc = 0; uLoc < myGrid->rectilinearGridM.Num(); ++uLoc)
	{
		for (int32 vLoc = 0; vLoc <= myGrid->gridFrequency * 2; ++vLoc)
		{
			if (vLoc != myGrid->gridFrequency * 2)
			{
				//upperTriangle
				int32 vertU = uLoc;
				int32 vertV = vLoc;
				Triangles.Add(myGrid->rectilinearGridM[vertU][vertV]);
				++vertV;
				Triangles.Add(myGrid->rectilinearGridM[vertU][vertV]);
				--vertV;
				myGrid->decrementU(vertU, vertV);
				Triangles.Add(myGrid->rectilinearGridM[vertU][vertV]);
			}
			if (vLoc != 0)
			{
				//lowerTriangle
				int32 vertU = uLoc;
				int32 vertV = vLoc;
				Triangles.Add(myGrid->rectilinearGridM[vertU][vertV]);
				myGrid->decrementU(vertU, vertV);
				Triangles.Add(myGrid->rectilinearGridM[vertU][vertV]);
				--vertV;
				Triangles.Add(myGrid->rectilinearGridM[vertU][vertV]);
			}
		}
	}

	int32 targetMeshNum = numMeshes;
	if (meshToRebuild != -1)
	{
		targetMeshNum = meshToRebuild;
	}
	CreateMeshSection(targetMeshNum, Vertices, Triangles, Normals, UV0, vertexColors, Tangents, false);
	SetMaterial(targetMeshNum, newMeshMaterial);
	if (meshToRebuild != -1)
	{
		++numMeshes;
	}
	return targetMeshNum;
}

void UGridMesher::rebuildBaseMeshFromGrid()
{
	ClearAllMeshSections();
	numMeshes = 0;
	debugLineOut->Flush();

	if (myGrid != nullptr)
	{
		TArray<float> vertexRadii;
		vertexRadii.SetNumZeroed(myGrid->gridLocationsM.Num());
		TArray<FColor> vertexColors;
		vertexColors.SetNumZeroed(myGrid->gridLocationsM.Num());
		for (const FRectGridLocation& gridLoc : myGrid->gridLocationsM)
		{
			FVector tilePos = myGrid->getNodeLocationOnSphere(gridLoc);
			vertexRadii[gridLoc.tileIndex] = baseMeshRadius;
			vertexColors[gridLoc.tileIndex] = FColor::Blue;
			if (renderNodes)
			{
				debugLineOut->DrawPoint(tilePos * baseMeshRadius, FLinearColor::Blue, 8, 2);
			}
		}
		if (renderBaseMesh)
		{
			buildNewMesh(vertexRadii, vertexColors, baseMeshMaterial);
		}
	}
}

FVector UGridMesher::calculateVertexNormal(const FRectGridLocation& gridLoc, const TArray<float>& vertexRadii) const
{
	FVector tilePos = myGrid->getNodeLocationOnSphere(gridLoc) * vertexRadii[gridLoc.tileIndex];
	TArray<int32> tileNeighbors = myGrid->getTileNeighborIndexes(gridLoc);
	FVector vertexNormal(0.0,0.0,0.0);
	for (int32 neighborNum = 0; neighborNum < tileNeighbors.Num(); ++neighborNum)
	{
		int32 neighbor2Num = neighborNum + 1;
		if (neighbor2Num == tileNeighbors.Num())
		{
			neighbor2Num = 0;
		}
		vertexNormal += FVector::CrossProduct(myGrid->getNodeLocationOnSphere(myGrid->gridLocationsM[tileNeighbors[neighborNum]])*vertexRadii[tileNeighbors[neighborNum]],
			myGrid->getNodeLocationOnSphere(myGrid->gridLocationsM[tileNeighbors[neighbor2Num]])*vertexRadii[tileNeighbors[neighbor2Num]]);
	}
	vertexNormal /= tileNeighbors.Num();
	vertexNormal /= FMath::Sqrt(FVector::DotProduct(vertexNormal, vertexNormal));
	vertexNormal *= -1; //need it to face out
	return vertexNormal;
}

