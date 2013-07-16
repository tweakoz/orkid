////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/file/path.h>
#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/collada/daeutil.h>
#include <ork/application/application.h>

///////////////////////////////////////////////////////////////////////////////

DaeDataSource::DaeDataSource( const FCDGeometrySource * pcolladasrc, FCDGeometryPolygons * MatGroup )
	: mSource( 0 )
	, mMatGroup( 0 )
	, miStride( 0 )
	, miCount( 0 )
	, mpIndices( 0 )
{
	if( pcolladasrc!=0 && MatGroup!=0 )
	{
		Bind( pcolladasrc, MatGroup );
	}
}

void DaeDataSource::Bind( const FCDGeometrySource * pcolladasrc, FCDGeometryPolygons * MatGroup )
{
	//mData = pcolladasrc ? pcolladasrc->GetDataPtr() : 0;
	mSource = pcolladasrc;
	mMatGroup = MatGroup;

	//FCDGeometryPolygons::FindIndicesForIdx 
	//;
	FCDGeometryPolygonsInput* pinput = MatGroup->FindInput( pcolladasrc ); //MatGroup->GetInput(0);
	
	if( pinput )
	{
		miStride = pcolladasrc ? mSource->GetStride() : 0;
		miCount = mSource->GetValueCount();
		mpIndices = pcolladasrc ? pinput->GetIndices() : 0; //FindIndices(pcolladasrc) : 0;
	}
}

int DaeDataSource::GetSourceIndex( int ifacevertbase, int ivertinface ) const
{
	if( 0 == mpIndices ) return 0;
	int index = ifacevertbase+ivertinface;
	//int isrcidx = miStride*(mpIndices[ index ]);
	int isrcidx = miStride*(mpIndices[ index ]);
	return isrcidx;
}

size_t DaeDataSource::GetDataSize() const
{
	return miCount;
	//return mData ? mData->size() : 0;
}

float DaeDataSource::GetData( int idx ) const
{
	const auto& src_data = mSource->GetSourceData();
	return src_data[idx];
}

int DaeDataSource::GetStride() const { return miStride; }

///////////////////////////////////////////////////////////////////////////////

bool ParseColladaMaterialBindings( FCDocument& daedoc, orkmap<std::string,std::string>& MatSemMap )
{
	FCDVisualSceneNodeLibrary* VizSceneLib = daedoc.GetVisualSceneLibrary();

	size_t inumscenenodes = VizSceneLib->GetEntityCount();

	int inumtotalmatbindings = 0;

	orkstack<FCDSceneNode*>	NodeStack;

	for( size_t inode=0; inode<inumscenenodes; inode++ )
	{
		FCDSceneNode* EntNode = VizSceneLib->GetEntity(inode);

		NodeStack.push( EntNode );

		while( NodeStack.empty() == false )
		{
			FCDSceneNode* Node = NodeStack.top();
			NodeStack.pop();

			std::string NodeName = Node->GetName().c_str();

			size_t inumchild = Node->GetChildrenCount();

			for( size_t ichild=0; ichild<inumchild; ichild++ )
			{
				FCDSceneNode* Child = Node->GetChild(ichild);
				NodeStack.push( Child );
			}

			size_t inuminst = Node->GetInstanceCount();

			for( size_t inst=0; inst<inuminst; inst++ )
			{
				inumtotalmatbindings ++;

				FCDEntityInstance * pinst = Node->GetInstance( inst );

				//if( FCDEntityInstance::GEOMETRY == pinst->GetType() )
				if(pinst->HasType(FCDGeometryInstance::GetClassType()))
				{
					FCDGeometryInstance *pgeoinst = static_cast<FCDGeometryInstance*>(pinst);

					//FCDMaterialInstanceContainer & MaterialBindings = pgeoinst->GetMaterialInstances();
					//size_t inummat = MaterialBindings.size();
					size_t inummat = pgeoinst->GetMaterialInstanceCount();

					for( size_t imat=0; imat<inummat; imat++ )
					{
						//FCDMaterialInstance** MaterialBindings = pgeoinst->GetMaterialInstance(();
						FCDMaterialInstance *MaterialBinding = pgeoinst->GetMaterialInstance(imat);

						std::string Semantic = MaterialBinding->GetSemantic().c_str();
						FCDMaterial * Material = MaterialBinding->GetMaterial();

						if( 0 == Material )
						{
							return false;
						}

						if( Material )
						{
							std::string MatName = Material->GetName().c_str();

							size_t inumbind = MaterialBinding->GetBindingCount();

							//if( inumbind )
							{
								MatSemMap[ Semantic ] = MatName;
							}
							//else
							//{
							//	orkprintf( "WARNING: shading group<%s> material<%s> not used!!\n", Semantic.c_str(), MatName.c_str() );
							//}

							for( size_t ibind=0; ibind<inumbind; ibind++ )
							{
								FCDMaterialInstanceBind *Binding = MaterialBinding->GetBinding( ibind );

								const FCDMaterialInstanceBind::Parameter_semantic & Semantic = Binding->semantic;
								const FCDMaterialInstanceBind::Parameter_target & Target = Binding->target;

							}
						}
					}
				}
			}
		}
	}
	if( 0 == inumtotalmatbindings )
	{
		return false;
	}
	return true;
}
