////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/RacingLineData.h>

#include <pkg/ent/bullet.h>
#include <pkg/ent/bullet_sector.h>

#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/kernel/orklut.hpp>

#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::bullet::RacingLine, "RacingLine");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::bullet::RacingLineSample, "RacingLineSample");

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::bullet::RacingLineData, "RacingLineData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::bullet::RacingLineInst, "RacingLineInst");

namespace ork { namespace ent { namespace bullet {

void RacingLine::Describe()
{
	ork::reflect::RegisterProperty("Time", &RacingLine::mTime);

	ork::reflect::RegisterMapProperty("RacingLineSamples", &RacingLine::mRacingLineSamples);
	reflect::AnnotatePropertyForEditor<RacingLine>("RacingLineSamples", "editor.map.policy.const", "true");
}

RacingLine::~RacingLine()
{
	for( orklut<float, RacingLineSample *>::const_iterator
			it=mRacingLineSamples.begin();
			it!=mRacingLineSamples.end();
			it++
				)
	{
		RacingLineSample* psample = it->second;
		delete psample;
	}

}

void RacingLineSample::Describe()
{
	ork::reflect::RegisterProperty("Position", &RacingLineSample::mPosition);
	ork::reflect::RegisterProperty("Thrust", &RacingLineSample::mThrust);
	ork::reflect::RegisterProperty("Brake", &RacingLineSample::mBrake);
	ork::reflect::RegisterProperty("Steering", &RacingLineSample::mSteering);
}

void RacingLineData::Describe()
{
	// so we can draw racing lines, let's get an update
	ork::ent::RegisterFamily<RacingLineData>(ork::AddPooledLiteral("control"));

	ork::reflect::RegisterMapProperty("RacingLines", &RacingLineData::mRacingLines);
	reflect::AnnotatePropertyForEditor<RacingLineData>("RacingLines", "editor.map.policy.const", "true");
}

RacingLineData::~RacingLineData()
{
	for( orklut<int, RacingLine *>::const_iterator
			it=mRacingLines.begin();
			it!=mRacingLines.end();
			it++
				)
	{
		RacingLine* pline = it->second;
		delete pline;
	}
}

const RacingLine *RacingLineData::GetRacingLine(int index) const
{
	orklut<int, RacingLine *>::const_iterator it = mRacingLines.find(index);
	OrkAssert(it != mRacingLines.end());
	return it->second;
}

ork::ent::ComponentInst *RacingLineData::CreateComponent(ork::ent::Entity *pent) const
{
	return OrkNew RacingLineInst(*this, pent);
}

static ork::PoolString sBulletWorldString;
static ork::PoolString sTrackString;

void RacingLineInst::Describe()
{
	sBulletWorldString = ork::AddPooledLiteral("bullet_world");
	sTrackString = ork::AddPooledLiteral("track");
}

RacingLineInst::RacingLineInst(const RacingLineData &data, ork::ent::Entity *pent)
	: ork::ent::ComponentInst(&data, pent)
	, mData(data)
	, mTrack(NULL)
{
}

static void LerpSamples(float progressPrev, const RacingLineSample &samplePrev,
						float progressNext, const RacingLineSample &sampleNext,
						float progress, RacingLineSample &sample,
						ork::CVector3 &racingLineDir)
{
	// Lerp
	float inverse_distance = 1.0f / (progressNext - progressPrev);
	float weight_next = (progress - progressPrev) * inverse_distance;
	float weight_prev = 1.0f - weight_next;
	sample.SetThrust(weight_next * sampleNext.GetThrust() + weight_prev * samplePrev.GetThrust());
	sample.SetBrake(weight_next * sampleNext.GetBrake() + weight_prev * samplePrev.GetBrake());
	sample.SetSteering(weight_next * sampleNext.GetSteering() + weight_prev * samplePrev.GetSteering());

	const ork::CMatrix4 &nextmatrix = sampleNext.GetPosition();
	const ork::CMatrix4 &prevmatrix = samplePrev.GetPosition();
	ork::CVector3 nextposition = nextmatrix.GetTranslation();
	ork::CVector3 prevposition = prevmatrix.GetTranslation();
	racingLineDir = (prevposition - nextposition).Normal();
	ork::CVector3 position = weight_next * nextposition + weight_prev * prevposition;

	ork::CMatrix4 matrix = nextmatrix;
	matrix.SetTranslation(position);
	sample.SetPosition(matrix);

	// TODO: Lerp orientation
}

void RacingLineInst::Sample(int racing_line_index, float progress, RacingLineSample &sample,
							ork::CVector3 &racingLineDir) const
{
	if(const ork::ent::bullet::RacingLine *racingLine = mData.GetRacingLine(racing_line_index))
	{
		const orklut<float, RacingLineSample *> &racingLineSampleLut
			= racingLine->GetRacingLineSamples();

		OrkAssert(racingLineSampleLut.size() > 0);

		orklut<float, RacingLineSample *>::const_iterator next = racingLineSampleLut.LowerBound(progress);
		if(next != racingLineSampleLut.end())
		{
			// there's a next
			if(next != racingLineSampleLut.begin())
			{
				// there's a previous
				orklut<float, RacingLineSample *>::const_iterator prev = next - 1;

				LerpSamples(prev->first, *prev->second, next->first, *next->second, progress, sample, racingLineDir);
			}
			else
			{
				// there's no previous, find it
				orklut<float, RacingLineSample *>::const_iterator prev = racingLineSampleLut.end() - 1;

				LerpSamples(prev->first, *prev->second, next->first, *next->second, progress, sample, racingLineDir);
			}
		}
		else
		{
			// there's no next, find it
			orklut<float, RacingLineSample *>::const_iterator next = racingLineSampleLut.begin();
			orklut<float, RacingLineSample *>::const_iterator prev = racingLineSampleLut.end() - 1;

			LerpSamples(prev->first, *prev->second, next->first, *next->second, progress, sample, racingLineDir);
		}
	}
}

bool RacingLineData::PostDeserialize(reflect::IDeserializer &)
{
	int re_idx = 0;
	for(orklut<int, RacingLine *>::iterator it = mRacingLines.begin();
			it != mRacingLines.end(); it++)
	{
		it->first = re_idx;
		re_idx++;

	}

	if( mRacingLines.size() > 6 )
	{
		orklut<int, RacingLine *> templut = mRacingLines;
		mRacingLines.Clear();
		for( int i=0; i<6; i++ )
		{
			orkprintf( "resize racingline(%d:%p)\n", i, templut.RefIndex(i).second );
			mRacingLines.AddSorted( i, templut.RefIndex(i).second );

		}

	}

	return true;
}

bool RacingLineInst::DoLink(ork::ent::SceneInst *sinst)
{
	ork::ent::Entity *trackEntity = sinst->FindEntityLoose(sTrackString);
	if( 0 == trackEntity ) return false;
	ork::ent::bullet::TrackInst* trackInst = trackEntity->GetTypedComponent<ork::ent::bullet::TrackInst>(true);
	if( 0 == trackInst ) return false;
	mTrack = &trackInst->GetTrack();

	ork::ent::Entity *bullet_world = sinst->FindEntityLoose(sBulletWorldString);
	if( 0 == bullet_world ) return false;
	ork::ent::BulletWorldControllerInst *world_controller
		= bullet_world->GetTypedComponent<ork::ent::BulletWorldControllerInst>(true);
	if( 0 == world_controller ) return false;

	mPhysicsDebugger = &world_controller->Debugger();
	if( 0 == mPhysicsDebugger ) return false;

	return true;
}

void RacingLineInst::DoUpdate(ork::ent::SceneInst *sinst)
{
	if(mPhysicsDebugger->getDebugMode())
	{
		bool clearOnBeginInternalTick = mPhysicsDebugger->IsClearOnBeginInternalTick();
		mPhysicsDebugger->ClearOnRender();

		const orklut<int, RacingLine *> &racingLineLut
			= mData.GetRacingLines();
		for(orklut<int, RacingLine *>::const_iterator it = racingLineLut.begin();
				it != racingLineLut.end(); it++)
		{
			RacingLine *racingLine = it->second;
			btVector3 current_position;
			const orklut<float, RacingLineSample *> &racingLineSampleLut = racingLine->GetRacingLineSamples();
			for(orklut<float, RacingLineSample *>::const_iterator it2 = racingLineSampleLut.begin();
					it2 != racingLineSampleLut.end(); it2++)
			{
				RacingLineSample *racingLineSample = it2->second;
				const ork::CMatrix4 &matrix = racingLineSample->GetPosition();
				ork::CVector3 position = matrix.GetTranslation();
				btVector3 next_position = !position;

				if(it2 != racingLineSampleLut.begin())
				{
					static const btVector3 colors[6] =
					{
						btVector3(0, 0, 0), btVector3(255, 0, 0), btVector3(0, 255, 0),
						btVector3(255, 255, 0), btVector3(0, 0, 255), btVector3(255, 0, 255)
					};
					btVector3 color(255, 255, 255);
					if(it->first < 6)
						color = colors[it->first];
					else
						orkprintf("AIlineidx: %d\n",it->first);
					mPhysicsDebugger->drawLine(current_position, next_position, color);
				}

				current_position = next_position;
			}
		}

		mPhysicsDebugger->SetClearOnBeginInternalTick(clearOnBeginInternalTick);
	}
}

} } }
