/*
    Copyright (C) 2005-2007 Feeling Software Inc.
    MIT License: http://www.opensource.org/licenses/mit-license.php
*/
#if 0
#include "StdAfx.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometryPolygonsSpecial.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FUtils/FUDaeEnumSyntax.h"
#include "FUtils/FUDaeParser.h"
#include "FUtils/FUDaeWriter.h"
#include "FUtils/FUStringConversion.h"
using namespace FUDaeParser;
using namespace FUDaeWriter;

//
// FCDGeometryPolygonsSpecial
//

ImplementObjectType(FCDGeometryPolygonsSpecial);

FCDGeometryPolygonsSpecial::FCDGeometryPolygonsSpecial(FCDocument* document, FCDGeometryMesh* _parent, PrimitiveType _type)
:	FCDGeometryPolygons(document, _parent)
{
	type = _type;
}

FCDGeometryPolygonsSpecial::~FCDGeometryPolygonsSpecial()
{
}


// Creates a new face.
void FCDGeometryPolygonsSpecial::AddFace(uint32 UNUSED(degree))
{
	uint32 _degree = 0; //just so that users of this function aren't confused

	// Inserts empty indices
	for (FCDGeometryPolygonsInputTrackList::iterator it = idxOwners.begin(); it != idxOwners.end(); ++it)
	{
		FCDGeometryPolygonsInput* input = (*it);
		if(input->indices.size() == 0)
		{
			if(type == FCDGeometryPolygons::TRIANGLE_FANS || type == FCDGeometryPolygons::TRIANGLE_STRIPS)
				_degree = 3;
			else if(type == FCDGeometryPolygons::LINE_STRIPS || type == FCDGeometryPolygons::LINES)
				_degree = 2;
		}
		else
		{
			if(type == FCDGeometryPolygons::TRIANGLE_FANS || type == FCDGeometryPolygons::TRIANGLE_STRIPS ||
				type == FCDGeometryPolygons::LINE_STRIPS )
				_degree = 1;
			else if(type == FCDGeometryPolygons::LINES)
				_degree = 2;
		}
		input->indices.resize(input->indices.size() + _degree, 0);
	}

	parent->Recalculate();
	SetDirtyFlag();
}

// Removes a face
void FCDGeometryPolygonsSpecial::RemoveFace(size_t index)
{
	FUAssert(index < GetFaceCount(), return);

	// Remove the associated indices, if they exist.
	size_t offset = GetFaceVertexOffset(index);

	for (FCDGeometryPolygonsInputTrackList::iterator it = idxOwners.begin(); it != idxOwners.end(); ++it)
	{
		FCDGeometryPolygonsInput* input = (*it);
		size_t inputIndexCount = input->indices.size();
		if (offset < inputIndexCount)
		{
			UInt32List::iterator end, begin = input->indices.begin() + offset;
			if(type == FCDGeometryPolygons::TRIANGLE_FANS || type == FCDGeometryPolygons::TRIANGLE_STRIPS ||
				type == FCDGeometryPolygons::LINE_STRIPS)
				end = input->indices.end(); //for strips and fans, remove everything until the end
			else if(type == FCDGeometryPolygons::LINES)
				end = begin + 2;

			input->indices.erase(begin, end);
		}
	}

	parent->Recalculate();
	SetDirtyFlag();
}

// Calculates the offset of face-vertex pairs before the given face index within the polygon set.
size_t FCDGeometryPolygonsSpecial::GetFaceVertexOffset(size_t index) const
{
	size_t offset = 0;

	if(index != 0)
	{
		if(type == FCDGeometryPolygons::TRIANGLE_FANS || type == FCDGeometryPolygons::TRIANGLE_STRIPS) 
			offset = 3 + index;
		else if(type == FCDGeometryPolygons::LINES)
			offset = index*2;
		else if(type == FCDGeometryPolygons::LINE_STRIPS)
			offset = 2 + index;
	}

	return offset;
}

size_t FCDGeometryPolygonsSpecial::GetFaceCount() const
{ 
	if( type == FCDGeometryPolygons::TRIANGLE_FANS || type == FCDGeometryPolygons::TRIANGLE_STRIPS )
		return faceVertexCount - 2;
	else if( type == FCDGeometryPolygons::LINES )
		return faceVertexCount / 2;
	else if ( type == FCDGeometryPolygons::LINE_STRIPS )
		return faceVertexCount - 1;

	return 0;
}


// The number of face-vertex pairs for a given face.
size_t FCDGeometryPolygonsSpecial::GetFaceVertexCount(size_t index) const
{
	size_t count = 0;
	if (index < GetFaceCount())
	{
		if( type == FCDGeometryPolygons::TRIANGLE_FANS || type == FCDGeometryPolygons::TRIANGLE_STRIPS )
		{
			if(index == 0) return 3;
			else return 1;
		}
		else if( type == FCDGeometryPolygons::LINES )
		{
			return 2;
		}
		else if ( type == FCDGeometryPolygons::LINE_STRIPS )
		{
			if(index == 0) return 2;
			else return 1;
		}
	}
	return count;
}


bool FCDGeometryPolygonsSpecial::LoadFromXML(xmlNode* baseNode)
{
	bool status = true;

	// the Collada spec doesn't seem to make use of count to indicate the number of triangles
	// in the strip or fan, but rather the number of fans or strips. There is no way to estimate
	// the face count in advance.

	// Check the node's name to know whether to expect a <vcount> element
	size_t expectedVertexCount = 1;
	if (!IsEquivalent(baseNode->name, DAE_TRIFANS_ELEMENT) && !IsEquivalent(baseNode->name, DAE_TRISTRIPS_ELEMENT)
		&& !IsEquivalent(baseNode->name, DAE_LINES_ELEMENT) && !IsEquivalent(baseNode->name, DAE_LINESTRIPS_ELEMENT) )
	{
		//return status.Fail(FS("Unknown polygons element in geometry: ") + TO_FSTRING(parent->GetDaeId()), baseNode->line);
		FUError::Error(FUError::ERROR, FUError::ERROR_UNKNOWN_POLYGONS, baseNode->line);
	}

	// Retrieve the material symbol used by these polygons
	materialSemantic = TO_FSTRING(ReadNodeProperty(baseNode, DAE_MATERIAL_ATTRIBUTE));
	if (materialSemantic.empty())
	{
		FUError::Error(FUError::WARNING, FUError::WARNING_INVALID_POLYGON_MAT_SYMBOL, baseNode->line);
	}

	// Read in the per-face, per-vertex inputs
	uint32 idxCount = 1;
	xmlNode* itNode = NULL;
	bool hasVertexInput = false;
	for (itNode = baseNode->children; itNode != NULL; itNode = itNode->next)
	{
		if (itNode->type != XML_ELEMENT_NODE) continue;
		if (IsEquivalent(itNode->name, DAE_INPUT_ELEMENT))
		{
			fm::string sourceId = ReadNodeSource(itNode);
			if (sourceId[0] == '#') sourceId.erase(0, 1);

			// Parse input idx/offset
			fm::string idx = ReadNodeProperty(itNode, DAE_OFFSET_ATTRIBUTE);
			uint32 offset = (!idx.empty()) ? FUStringConversion::ToUInt32(idx) : idxCount;
			idxCount = max(offset + 1, idxCount);

			// Parse input set
			fm::string setString = ReadNodeProperty(itNode, DAE_SET_ATTRIBUTE);
			uint32 set = setString.empty() ? -1 : FUStringConversion::ToInt32(setString);

			// Parse input semantic
			FUDaeGeometryInput::Semantic semantic = FUDaeGeometryInput::FromString(ReadNodeSemantic(itNode));
			if (semantic == FUDaeGeometryInput::UNKNOWN) continue; // Unknown input type
			else if (semantic == FUDaeGeometryInput::VERTEX)
			{
				// There should never be more than one 'VERTEX' input.
				if (hasVertexInput)
				{
					FUError::Error(FUError::WARNING, FUError::WARNING_EXTRA_VERTEX_INPUT, itNode->line);
					continue;
				}
				hasVertexInput = true;

				// Add an input for all the vertex sources in the parent.
				FCDGeometrySourceTrackList& vertexSources = parent->GetVertexSources();
				size_t vertexSourceCount = vertexSources.size();
				for (uint32 i = 0; i < vertexSourceCount; ++i)
				{
					FCDGeometryPolygonsInput* vertexInput = AddInput(vertexSources[i], offset);
					vertexInput->SetSet(set);
				}
			}
			else
			{
				// Retrieve the source for this input
				FCDGeometrySource* source = parent->FindSourceById(sourceId);
				if (source != NULL)
				{
					// The source may have a dangling type: the input sets contains that information in the COLLADA document.
					// So: enforce the source type of the input to the data source.
					source->SetSourceType(semantic); 
					FCDGeometryPolygonsInput* input = AddInput(source, offset);
					input->SetSet(set);
				}
				else
				{
					FUError::Error(FUError::WARNING, FUError::WARNING_UNKNOWN_POLYGONS_INPUT, itNode->line);
				}
			}
		}
		else if (IsEquivalent(itNode->name, DAE_POLYGON_ELEMENT)
			|| IsEquivalent(itNode->name, DAE_VERTEXCOUNT_ELEMENT)
			|| IsEquivalent(itNode->name, DAE_POLYGONHOLED_ELEMENT))
		{
			break;
		}
		else
		{
			FUError::Error(FUError::WARNING, FUError::WARNING_UNKNOWN_POLYGON_CHILD, itNode->line);
		}
	}
	if (itNode == NULL)
	{
		//return status.Fail(FS("No polygon <p>/<vcount> element found in geometry: ") + TO_FSTRING(parent->GetDaeId()), baseNode->line);
		FUError::Error(FUError::ERROR, FUError::ERROR_NO_POLYGON, baseNode->line);
	}
	if (!hasVertexInput)
	{
		// Verify that we did find a VERTEX polygon set input.
		//return status.Fail(FS("Cannot find 'VERTEX' polygons' input within geometry: ") + TO_FSTRING(parent->GetDaeId()), baseNode->line);
		FUError::Error(FUError::ERROR, FUError::ERROR_NO_VERTEX_INPUT, baseNode->line);
	}

	// Look for the <vcount> element and fail if it's there
	xmlNode* vCountNode = FindChildByType(baseNode, DAE_VERTEXCOUNT_ELEMENT);
	if (vCountNode)
	{
		//return status.Fail(FS("<vcount> is only expected with the <polylist> element in geometry: ") + TO_FSTRING(parent->GetDaeId()), baseNode->line);
		FUError::Error(FUError::ERROR, FUError::ERROR_MISPLACED_VCOUNT, baseNode->line);
	}

	// Pre-allocate the buffers with enough memory
	UInt32List allIndices;
	faceVertexCount = 0;
	allIndices.clear();
	allIndices.reserve( 3 + (expectedVertexCount-1) * idxCount); //first triangle is 3, then each new one is 1
	FCDGeometryPolygonsInputContainer::iterator it = idxOwners.begin();
	(*it)->indices.reserve(3);
	for (it = idxOwners.begin()+1; it != idxOwners.end(); ++it)
	{
		(*it)->indices.reserve(expectedVertexCount);
	}

	uint32 numberOfElements = ReadNodeCount(baseNode);

	// Process the tessellation
	for (; itNode != NULL; itNode = itNode->next)
	{
		uint32 localFaceVertexCount = 0;
		const char* content = NULL;
		bool failed = false;
		xmlNode* dummy = NULL;
		if (!InitTessellation(itNode, &localFaceVertexCount, allIndices, content, dummy, idxCount, &failed)) continue;

		faceVertexCount += localFaceVertexCount;
		numberOfVerticesPerList.push_back((uint32)allIndices.size());
		
		if(type == TRIANGLE_FANS || type == TRIANGLE_STRIPS || type == LINE_STRIPS)
			numberOfElements--;
		else
			numberOfElements -= localFaceVertexCount;

		// Create a new entry for the vertex buffer
		for (size_t offset = 0; offset < allIndices.size(); offset += idxCount)
		{
			for (FCDGeometryPolygonsInputContainer::iterator it = idxOwners.begin(); it != idxOwners.end(); ++it)
			{
				(*it)->indices.push_back(allIndices[offset + (*it)->idx]);
			}
		}

	}

	// Check the actual element count
	if (numberOfElements != 0)
	{
		FUError::Error(FUError::WARNING, FUError::WARNING_INVALID_PRIMITIVE_COUNT, baseNode->line);
		return status;
	}

	SetDirtyFlag();
	return status;
}

bool FCDGeometryPolygonsSpecial::InitTessellation(xmlNode* itNode, 
		uint32* localFaceVertexCount, UInt32List& allIndices, 
		const char* content, xmlNode*& UNUSED(holeNode), uint32 UNUSED(idxCount), 
		bool* UNUSED(failed))
{
	if (itNode->type != XML_ELEMENT_NODE) return false;
	if (!IsEquivalent(itNode->name, DAE_POLYGON_ELEMENT)) return false;

	// Retrieve the indices
	content = ReadNodeContentDirect(itNode);

	// Parse the indices
	allIndices.clear();
	FUStringConversion::ToUInt32List(content, allIndices);
	if(type == TRIANGLE_FANS || type == TRIANGLE_STRIPS) 
		*localFaceVertexCount = (uint32) allIndices.size() - 2;
	else if(type == LINE_STRIPS)
		*localFaceVertexCount = (uint32) allIndices.size() - 1;
	else if(type == LINES)
		*localFaceVertexCount = (uint32) allIndices.size() /2;

	SetDirtyFlag();
	return true;
}

// Write out the polygons structure to the COLLADA XML tree
xmlNode* FCDGeometryPolygonsSpecial::WriteToXML(xmlNode* parentNode) const
{
	// Create the base node for these polygons
	const char* polygonNodeType = NULL;
	if(type == TRIANGLE_FANS) polygonNodeType = DAE_TRIFANS_ELEMENT;
	else if(type == TRIANGLE_STRIPS) polygonNodeType = DAE_TRISTRIPS_ELEMENT;
	else return NULL;
	
	xmlNode* polygonsNode = AddChild(parentNode, polygonNodeType);

	// Add the inputs
	// Find which input owner belongs to the <vertices> element. Replace the semantic and the source id accordingly.
	// Make sure to add that 'vertex' input only once.
	FUSStringBuilder verticesNodeId(parent->GetDaeId()); verticesNodeId.append("-vertices");
	const FCDGeometrySourceTrackList& vertexSources = parent->GetVertexSources();
	bool isVertexInputFound = false;
	for (FCDGeometryPolygonsInputContainer::const_iterator itI = inputs.begin(); itI != inputs.end(); ++itI)
	{
		const FCDGeometryPolygonsInput* input = *itI;
		const FCDGeometrySource* source = input->GetSource();
		if (source != NULL)
		{
			if (vertexSources.find(source) == vertexSources.end())
			{
				const char* semantic = FUDaeGeometryInput::ToString(input->GetSemantic());
				FUDaeWriter::AddInput(polygonsNode, source->GetDaeId(), semantic, input->idx, input->GetSet());
			}
			else if (!isVertexInputFound)
			{
				FUDaeWriter::AddInput(polygonsNode, verticesNodeId.ToCharPtr(), DAE_VERTEX_INPUT, input->idx);
				isVertexInputFound = true;
			}
		}
	}

	FUSStringBuilder builder;
	builder.reserve(1024);

	// open as many necessary <p> element as needed for the data indices
	uint32 counter = 0;
	for(UInt32List::const_iterator it = numberOfVerticesPerList.begin(); it != numberOfVerticesPerList.end(); ++it)
	{
		builder.clear();
		xmlNode* pNode = AddChild(polygonsNode, DAE_POLYGON_ELEMENT);
		uint32 numElements = *it;
		for(uint32 i=0; i<numElements; ++i)
		{
			for (FCDGeometryPolygonsInputTrackList::const_iterator itI = idxOwners.begin(); itI != idxOwners.end(); ++itI)
			{
				const UInt32List& indices = (*itI)->indices;
				builder.append(indices[counter+i]);
				builder.append(' ');
			}
		}
		counter+=numElements;

		// write out the indices at the very end, for the single <p> element
		if (!builder.empty()) builder.pop_back(); // take out the last space
		AddContent(pNode, builder);
	}

	// Write out the material semantic and the number of polygons
	if (!materialSemantic.empty())
	{
		AddAttribute(polygonsNode, DAE_MATERIAL_ATTRIBUTE, materialSemantic);
	}
	AddAttribute(polygonsNode, DAE_COUNT_ATTRIBUTE, GetFaceCount());

	return polygonsNode;
#endif}
