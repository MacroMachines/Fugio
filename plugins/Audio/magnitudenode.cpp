#include "magnitudenode.h"

#include <fugio/core/uuid.h>
#include <fugio/audio/uuid.h>

#include <fugio/context_interface.h>
#include <fugio/core/variant_interface.h>
#include <fugio/audio/fft_interface.h>
#include <fugio/audio/audio_producer_interface.h>

#include <fugio/context_signals.h>

#include <qmath.h>

#define MAG_SMP	(48000/50)

MagnitudeNode::MagnitudeNode( QSharedPointer<fugio::NodeInterface> pNode )
	: NodeControlBase( pNode ), mMagnitude( 0 ), mSamplePosition( 0 ), mProducerInstance( nullptr )
{
	mPinAudio = pinInput( "Audio" );

	mPinSampleCount = pinInput( "Samples" );

	mPinSampleCount->setValue( MAG_SMP );

	mValOutput = pinOutput<fugio::VariantInterface *>( "Magnitude", mPinOutput, PID_FLOAT );

	mPinAudio->setDescription( tr( "The source audio signal" ) );

	mPinSampleCount->setDescription( tr( "The number of samples to count in each calculation" ) );

	mPinOutput->setDescription( tr( "The largest magnitude of the audio signal" ) );
}

bool MagnitudeNode::initialise()
{
	if( !fugio::NodeControlBase::initialise() )
	{
		return( false );
	}

	connect( mNode->context()->qobject(), SIGNAL(frameProcess(qint64)), this, SLOT(onContextProcess(qint64)) );

	return( true );
}

bool MagnitudeNode::deinitialise()
{
	disconnect( mNode->context()->qobject(), SIGNAL(frameProcess(qint64)), this, SLOT(onContextProcess(qint64)) );

	return( NodeControlBase::deinitialise() );
}

void MagnitudeNode::onContextProcess( qint64 pTimeStamp )
{
	if( !pTimeStamp )
	{
		mSamplePosition = 0;

		return;
	}

	if( !mSamplePosition )
	{
		mSamplePosition = pTimeStamp * ( 48000 / 1000 );
	}

#if 0
	qint64	CurPos = pTimeStamp * ( 48000 / 1000 );

	if( CurPos - mSamplePosition >= MAG_SMP )
	{
		InterfaceAudioProducer	*AP = input<InterfaceAudioProducer *>( mPinAudio );

		if( AP )
		{
			if( !mProducerInstance )
			{
				mProducerInstance = AP->allocAudioInstance();
			}

			if( AP->channels() != mAudioData.mSampleData.size() )
			{
				mAudioData.mSampleData.resize( AP->channels() );

				for( auto &V : mAudioData.mSampleData )
				{
					V.resize( 48000 );
				}
			}

			QVector<float *>	AudPtr;

			AudPtr.resize( AP->channels() );

			for( int i = 0 ; i < AP->channels() ; i++ )
			{
				AudPtr[ i ] = mAudioData.mSampleData[ i ].data();

				memset( AudPtr[ i ], 0, sizeof( float ) * MAG_SMP );
			}

			AP->audio( mSamplePosition, MAG_SMP, 0, 1, AudPtr.data(), 0, mProducerInstance );

			float		Mag = 0;

			for( int c = 0 ; c < AP->channels() ; c++ )
			{
				float	*S = AudPtr[ c ];

				for( int i = 0 ; i < MAG_SMP ; i++, S++ )
				{
					float	M = fabs( *S );

					if( M > Mag )
					{
						Mag = M;
					}
				}
			}

			mMagnitude = Mag;
		}

		mSamplePosition += MAG_SMP;
	}

	if( mMagnitude != mValOutput->variant().toFloat() )
	{
		mValOutput->setVariant( mMagnitude );

		pinUpdated( mPinOutput );
	}
#endif
}
