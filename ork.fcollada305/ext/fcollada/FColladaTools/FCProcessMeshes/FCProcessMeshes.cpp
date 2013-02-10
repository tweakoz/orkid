/*
    Copyright (C) 2005-2007 Feeling Software Inc.
    MIT License: http://www.opensource.org/licenses/mit-license.php

    Portions of the code surrounded by PREMIUM conditional compilation
    blocks are copyright Feeling Software Inc., strictly confidential and released
    to Premium Support licensees only. Licensees are allowed to modify and extend
    the Premium code for their internal use. Redistributing the Premium source code
    or any of its derivative product (e.g. binary versions) is forbidden.
*/

/*
	FCProcessMeshes runs selected polygons tools of the meshes of a
	COLLADA document.
*/

#include "StdAfx.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDAsset.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometryPolygons.h"
#include "FCDocument/FCDGeometryPolygonsTools.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDExtra.h"
#include "FUtils/FUFileManager.h"

struct ProcessMeshesOptions
{
	bool fixModel;
	bool textureTangents;
	bool triangulate;
};

void ProcessGeometryLibrary(FCDGeometryLibrary* library, const ProcessMeshesOptions& options);
void ProcessMesh(FCDGeometryMesh* mesh, const ProcessMeshesOptions& options);

void PrintUsage()
{
	std::cout << "Expecting two arguments:" << std::endl;
	std::cout << "FCProcessMeshes.exe [-fm][-t][-tt] <input_filename> <output_filename>" <<std::endl;
	std::cout << "-fm Fix model for the viewer so there is less need for runtime processing." <<std::endl;
	std::cout << "-t Triangulate the meshes." <<std::endl;
	std::cout << "-tt Generate texture tangents for the meshes. This implies triangulating." <<std::endl;
}

int main(int argc, const char* argv[], char* envp[])
{
#ifdef WIN32
	_environ = envp;
#else //LINUX
	environ = envp;
#endif //WIN32 and LINUX

	// variables for processing
	ProcessMeshesOptions options;
	options.fixModel = false;
	options.textureTangents = false;
	options.triangulate = false;
	fstring inputFilename;
	fstring outputFilename;

	// parse the arguments
	int argCounter = 1;
	while (argCounter < argc)
	{
		if (argv[argCounter][0] == '-')
		{
			if (IsEquivalent(argv[argCounter], "-fm"))
			{
				options.fixModel = true;
			}
			else if (IsEquivalent(argv[argCounter], "-t"))
			{
				options.triangulate = true;
			}
			else if (IsEquivalent(argv[argCounter], "-tt"))
			{
				options.textureTangents = true;
			}
			else 
			{
				PrintUsage();
				exit(-1);
			}
			argCounter++;
		}
		else
		{
			// should be last two arguments, the file name
			if (argc - argCounter != 2)
			{
				PrintUsage();
				exit(-1);
			}
			inputFilename = TO_FSTRING(argv[argCounter]);
			outputFilename = TO_FSTRING(argv[argCounter + 1]);
			argCounter += 2;
		}
	}
	if (inputFilename.empty() || outputFilename.empty())
	{
		PrintUsage();
		exit(-1);
	}

	std::cout << argv[1] << std::endl;
	std::cout << "Import: ";
	std::cout.flush();

	// Create an empty COLLADA document and import the given file.
	FCollada::Initialize();
	FCDocument* document = FCollada::NewTopDocument();
	FUErrorSimpleHandler errorHandler;
	FCollada::LoadDocumentFromFile(document, inputFilename.c_str());
	if (errorHandler.IsSuccessful())
	{
		std::cout << "Done." << std::endl;
		std::cout << "Processing: "; std::cout.flush();

		ProcessGeometryLibrary(document->GetGeometryLibrary(), options);

		// It is common practice for tools to add a new contributor to identify that they were run
		// on a COLLADA document.
		FCDAssetContributor* contributor = document->GetAsset()->AddContributor();
		const char* userName = getenv("USER");
		if (userName == NULL) userName = getenv("USERNAME");
		if (userName != NULL) contributor->SetAuthor(TO_FSTRING(userName));
		contributor->SetSourceData(inputFilename);
		char authoringTool[1024];
		snprintf(authoringTool, 1024, "FCProcessMeshes sample for FCollada v%d.%02d", FCOLLADA_VERSION >> 16, FCOLLADA_VERSION & 0xFFFF);
		authoringTool[1023] = 0;
		contributor->SetAuthoringTool(TO_FSTRING((const char*)authoringTool));

		// Write out the processed COLLADA document.
		std::cout << "Done." << std::endl;
		std::cout << "Export: "; std::cout.flush();
		FCollada::SaveDocument(document, outputFilename.c_str());
		if (errorHandler.IsSuccessful())
		{
			std::cout << "Done." << std::endl;
		}
		else
		{
			std::cout << errorHandler.GetErrorString();
			std::cout << std::endl << std::endl;
		}
	}
	else
	{
		std::cout << errorHandler.GetErrorString();
		std::cout << std::endl << std::endl;
	}

	SAFE_DELETE(document);
	FCollada::Release();
	return 0;
}

void ProcessGeometryLibrary(FCDGeometryLibrary* library, const ProcessMeshesOptions& options)
{
	size_t geometryCount = library->GetEntityCount();
	for (size_t i = 0; i < geometryCount; ++i)
	{
		FCDGeometry* geometry = library->GetEntity(i);
		if (geometry->IsMesh())
		{
			ProcessMesh(geometry->GetMesh(), options);
		}
	}
}

void ProcessMesh(FCDGeometryMesh* mesh, const ProcessMeshesOptions& options)
{
	if (options.triangulate || options.textureTangents)
	{
		FCDGeometryPolygonsTools::Triangulate(mesh);
	}

	if (options.textureTangents)
	{
		FCDGeometrySource* texcoordSource = mesh->FindSourceByType(FUDaeGeometryInput::TEXCOORD);
		if (texcoordSource != NULL)
		{
			// Generate the texture tangents for this mesh' texcoords.
			FCDGeometryPolygonsTools::GenerateTextureTangentBasis(mesh, texcoordSource, true);
		}
	}

	// taken from FRMesh::TranslateFromFCD to determine what qualifies as fixedModel
	if (options.fixModel)
	{
		FCDGeometryPolygonsTools::GenerateUniqueIndices(mesh);
		FCDGeometryPolygonsTools::FitIndexBuffers(mesh, 1024*48); // 2 ^ 16 - 1: this is a hardware drawback.
	}
}
