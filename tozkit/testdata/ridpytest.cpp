#include "rihelper.h"
#include <stdio.h>

/////////////////////////////////////////////////////////

void CreateShaders(ShaderMap&shmap)
{
	///////////////////////////////////
	{
		Shader surface("surface","constant");
		shmap["constant"]=surface;
	}
	///////////////////////////////////
	{
		Shader smoke("atmosphere","smoke");
		smoke["opacdensity"]=1.0f;
		smoke["lightdensity"]=0.2f;
		smoke["integstart"]=0.0f;
		smoke["integend"]=1000.1f;
		smoke["stepsize"]=0.10f;
		smoke["smokeoctaves"]=4;
		smoke["scatter"]=c3f(0.25f,0.25f,0.15f);
		shmap["smoke"]=smoke;
	}
	///////////////////////////////////
	{
		Shader surface("surface","matte");
		shmap["matte"]=surface;
	}
	///////////////////////////////////
	{
		Attributes ss_attrs("ri:subsurface");
		ss_attrs["visibility"]=str_t("marble");
		ss_attrs["scattering"]=c3f(2.19,2.62,1.00);
		ss_attrs["absorption"]=c3f(0.0021,0.0041,0.0071);
		ss_attrs["refractionindex"]=1.3f;
		ss_attrs["shadingrate"]=8.0f;
		ss_attrs["scale"]=0.5f;

		Attributes bnd_attrs("ri:bound");
		bnd_attrs["displacement"]=0.3f;

		Shader surface("surface","simple");
		surface["Ks"]=0.7f;
		surface["roughness"]=0.02f;

		//Shader atmos("atmosphere","smoke");
		//atmos["background"]=c3f(0.0f,0.0f,1.0f);
		//surface["roughness"]=0.02f;

		Shader displacement("displacement","stucco");
		displacement["Km"]=0.3f;

		Attributes trace_attrs("ri:trace");
		trace_attrs["displacements"]=1;
		trace_attrs["inflategrids"]=0.5f;

		CompoundState compound;
		compound["attr_ss"]=ss_attrs;
		compound["attr_bnd"]=bnd_attrs;
		compound["shad_s"]=surface;
		compound["shad_d"]=displacement;
		//compound["atmos"]=atmos;
		compound["trac"]=trace_attrs;

		shmap["ss_milky"]=compound;
	}
	///////////////////////////////////
	{
		Light l("spotlight","1");
		l["intensity"]=85.0f;	
		l["shadowmap"]=str_t("raytrace");	
		l["coneangle"]=3.0f;	
		l["lightcolor"]=c3f(1.0f);	
		shmap["spotlight"]=l;
	}
	///////////////////////////////////
	{
		Attributes attrs("ri:visibility");
		attrs["transmission"]=str_t("Os");
		shmap["vis:transmission"]=attrs;
	}
	///////////////////////////////////
}
/////////////////////////////////////////////////////////

void gen_frame( int iframe, const m44& view_mtx, PointsPrimitive* pp )
{
	float ftime = float(iframe)/60.0f;

	char buffer[256];
	snprintf( buffer, sizeof(buffer), "./output/ridpytest_%04d", iframe );
	my_renderer renderer(buffer, true);;
	IECoreRI::Renderer* r = renderer.rib();
	
	printf( "Rendering Frame<%s>\n", buffer );

	ShaderMap shmap;
	CreateShaders(shmap);

	//////////////////////////////
	//	camera setup

	var_map cam_vmap;
	cam_vmap["projection"]=str_t("perspective");
	cam_vmap["projection:fov"]=105.0f;
	cam_vmap["resolution"]=v2i(1280,720);
	cam_vmap.camera(r,"cam1");

	//////////////////////////////

	Attributes dof_attrs("toz:dof");
	dof_attrs["fstop"]=3.3f;
	dof_attrs["focallength"]=0.5f;
	dof_attrs["focaldistance"]=1.3f;
	dof_attrs.Apply(r);

	//////////////////////////////

	r->worldBegin();
	{
		shmap.Apply(r,"smoke");

		r->setTransform( view_mtx );

		////////////////////////////
		// visibility
		////////////////////////////

		shmap.Apply(r,"vis:transmission");

		////////////////////////////
		// spotlight
		////////////////////////////

		shmap["spotlight"].Get<Light>()["from"]=v3f(0.0f,10.0f,-1.0f);
		shmap["spotlight"].Get<Light>()["to"]=v3f(0.0f,0.0f,0.0f);
		shmap.Apply(r,"spotlight");

		////////////////////////////
		// points/metaballs
		////////////////////////////
		
		if( 0 )
		{
			r->attributeBegin();
			r->transformBegin();
			{
				r->setAttribute("color",new Color3fData(c3f(0.4f,0.0f,0.0f)));
				shmap.Apply(r,"constant");
				r->points( pp->getNumPoints(), pp->variables );
			}
			r->transformEnd();
			r->attributeEnd();
		}

		////////////////////////////
		// atmosphere
		////////////////////////////

		if( 1 )
		{
			r->attributeBegin();
			r->transformBegin();
			{
				r->setAttribute("color",new Color3fData(c3f(1.0f,1.0f,1.0f)));
				r->setAttribute("opacity",new Color3fData(c3f(0.0f,0.0f,0.0f)));
				shmap.Apply(r,"constant");

				PrimitiveVariableMap sph_vars;
				m44 sph_trans;
				sph_trans.translate(v3f(0,0.0f,0.0f));
				//r->setTransform( sph_trans );
				//r->setTransform( view_mtx*sph_trans );
				r->setTransform( view_mtx*sph_trans );
				r->sphere( 10.0f, -1, 1, 360, sph_vars );


			}
			r->transformEnd();
			r->attributeEnd();
		}

		////////////////////////////
		// subsurface scattering sphere
		////////////////////////////

		if( 1 )
		{
			r->attributeBegin();
			r->transformBegin();
			{
				r->setAttribute("color",new Color3fData(c3f(1.0f,1.0f,1.0f)));

				auto& ss_milky = shmap["ss_milky"].Get<CompoundState>();
				auto& shad_d = ss_milky["shad_d"].Get<Shader>();

				float Kmd = -cosf(ftime*6.283*1.5f);
				float Km = (0.3f+Kmd*0.4f);
				shad_d["Km"]=Km;

				auto& bnd_attr = ss_milky["attr_bnd"].Get<Attributes>();
				bnd_attr["displacement"]=Km;

				shmap.Apply(r,"ss_milky");

				PrimitiveVariableMap sph_vars;
				m44 sph_trans;
				sph_trans.translate(v3f(0,2.0f,0.0f));
				//r->setTransform( sph_trans );
				//r->setTransform( view_mtx*sph_trans );
				r->setTransform( view_mtx*sph_trans );
				r->sphere( 1.5f, -1, 1, 360, sph_vars );


			}
			r->transformEnd();
			r->attributeEnd();
		}

		////////////////////////////
		// matte ground plane
		////////////////////////////

		r->attributeBegin();
		r->transformBegin();
		{
			m44 mesh_mtx;
			mesh_mtx.translate(v3f(0,0,0));
			r->setTransform( view_mtx*mesh_mtx );

			shmap.Apply(r,"matte");

			PrimitiveVariableMap mesh_data;
			std::vector<int> verts_per_face, vert_ids;
			std::vector<v3f> verts;
			verts_per_face.push_back(4);
			vert_ids.push_back(0);
			vert_ids.push_back(1);
			vert_ids.push_back(2);
			vert_ids.push_back(3);
			verts.push_back(v3f(-10.0f,-0.0f,-10.0f));
			verts.push_back(v3f(+10.0f,-0.0f,-10.0f));
			verts.push_back(v3f(+10.0f,-0.0f,+10.0f));
			verts.push_back(v3f(-10.0f,-0.0f,+10.0f));
			
			auto points = new V3fVectorData(verts);

			mesh_data["P"]=PrimitiveVariable(PrimitiveVariable::Interpolation::Vertex,points);

			r->mesh(	new IntVectorData(verts_per_face), 
						new IntVectorData(vert_ids),
						"linear", // interpolation
						mesh_data );


		}
		r->transformEnd();
		r->attributeEnd();

		////////////////////////////
		////////////////////////////

	}
	r->worldEnd();

	//////////////////////////////
}

int main( int argc, char** argv )
{
	std::vector<v3f> vv;
	for( int i=0; i<100; i++ )
	{
		float ix = float(rand()%256)/256.0f;
		float iy = float(rand()%256)/256.0f;
		float iz = float(rand()%256)/256.0f;
		v3f v( ix, iy, iz );
		vv.push_back(v);
	}
	PointsPrimitive* pp = GenPointsPrimF( vv, 0.1f );




	for( int i=0; i<600.0f; i++ )
	{
		float fi = float(i)/300.0f;
		float fs = sinf(fi*6.28f)*3.0f;
		float fc = cosf(fi*6.28f)*3.0f;

		printf( "FS<%f> FC<%f>\n", fs, fc );

		v3f eye_pos(fs,1.5f,fc);
		v3f tgt_pos(0.0f,2.0f,0.0f);
		v3f up_dir(0.0f,1.0f,0.0f);

		m44 view_mtx = GenLookAtMatrix(eye_pos,tgt_pos,up_dir);

		gen_frame(i,view_mtx,pp);
	}
}
