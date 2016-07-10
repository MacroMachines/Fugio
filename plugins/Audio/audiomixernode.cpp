#include "audiomixernode.h"
#include <fugio/node_signals.h>

#include "audiopin.h"

AudioMixerNode::AudioMixerNode( QSharedPointer<fugio::NodeInterface> pNode )
	: NodeControlBase( pNode )
{
	QSharedPointer<fugio::PinInterface>	P1 = pinInput( "Audio" );

	mValOutput = pinOutput<fugio::AudioProducerInterface *>( "Audio Mix", mPinOutput, PID_AUDIO );

	P1->setDescription( tr( "The first audio source" ) );

	mPinOutput->setDescription( tr( "The audio mix from all the input channels added together" ) );

	connect( mNode->qobject(), SIGNAL(pinLinked(QSharedPointer<fugio::PinInterface>,QSharedPointer<fugio::PinInterface>)), this, SLOT(pinLinked(QSharedPointer<fugio::PinInterface>,QSharedPointer<fugio::PinInterface>)) );

	connect( mNode->qobject(), SIGNAL(pinUnlinked(QSharedPointer<fugio::PinInterface>,QSharedPointer<fugio::PinInterface>)), this, SLOT(pinUnlinked(QSharedPointer<fugio::PinInterface>,QSharedPointer<fugio::PinInterface>)) );
}

AudioMixerNode::~AudioMixerNode( void )
{

}

void AudioMixerNode::audio( qint64 pSamplePosition, qint64 pSampleCount, int pChannelOffset, int pChannelCount, void **pBuffers, void *pInstanceData ) const
{
	AudioInstanceData		*InsDat = static_cast<AudioInstanceData *>( pInstanceData );

	if( !InsDat )
	{
		return;
	}

	InsDat->mMutex.lock();

	for( auto it = InsDat->mInstanceData.begin() ; it != InsDat->mInstanceData.end() ; it++ )
	{
		it.key()->audio( pSamplePosition, pSampleCount, pChannelOffset, pChannelCount, pBuffers, it.value() );
	}

	InsDat->mMutex.unlock();
}

QList<QUuid> AudioMixerNode::pinAddTypesInput() const
{
	QList<QUuid>	IdList;

	IdList << PID_AUDIO;

	return( IdList );
}

bool AudioMixerNode::canAcceptPin( fugio::PinInterface *pPin ) const
{
	return( pPin->direction() == PIN_OUTPUT );
}

void *AudioMixerNode::audioAllocInstance( qreal pSampleRate, fugio::AudioSampleFormat pSampleFormat, int pChannels )
{
	AudioInstanceData		*InsDat = new AudioInstanceData();

	if( InsDat )
	{
		InsDat->mSampleRate   = pSampleRate;
		InsDat->mSampleFormat = pSampleFormat;
		InsDat->mChannels     = pChannels;

		for( QSharedPointer<fugio::PinInterface> DstPin : mNode->enumInputPins() )
		{
			QSharedPointer<fugio::PinInterface>			SrcPin = DstPin->connectedPin();

			if( !SrcPin )
			{
				continue;
			}

			QSharedPointer<fugio::PinControlInterface>		C = SrcPin->control();

			if( !C )
			{
				continue;
			}

			fugio::AudioProducerInterface *A = qobject_cast<fugio::AudioProducerInterface *>( C->qobject() );

			if( !A )
			{
				continue;
			}

			InsDat->mMutex.lock();

			InsDat->mInstanceData.insert( A, A->audioAllocInstance( pSampleRate, pSampleFormat, pChannels ) );

			InsDat->mMutex.unlock();
		}

		mInstanceData << InsDat;
	}

	return( InsDat );
}

void AudioMixerNode::audioFreeInstance( void *pInstanceData )
{
	AudioInstanceData		*InsDat = static_cast<AudioInstanceData *>( pInstanceData );

	if( InsDat )
	{
		InsDat->mMutex.lock();

		for( auto it = InsDat->mInstanceData.begin() ; it != InsDat->mInstanceData.end() ; it++ )
		{
			it.key()->audioFreeInstance( it.value() );
		}

		InsDat->mMutex.unlock();

		mInstanceData.removeAll( InsDat );

		delete InsDat;
	}
}

void AudioMixerNode::pinLinked( QSharedPointer<fugio::PinInterface> pPinSrc, QSharedPointer<fugio::PinInterface> pPinDst )
{
	if( pPinSrc->direction() != PIN_INPUT )
	{
		return;
	}

	fugio::AudioProducerInterface		*IAP = pPinDst->hasControl() ? qobject_cast<fugio::AudioProducerInterface *>( pPinDst->control()->qobject() ) : nullptr;

	if( IAP )
	{
		for( AudioInstanceData *InsDat : mInstanceData )
		{
			InsDat->mMutex.lock();

			if( !InsDat->mInstanceData.contains( IAP ) )
			{
				InsDat->mInstanceData.insert( IAP, IAP->audioAllocInstance( InsDat->mSampleRate, InsDat->mSampleFormat, InsDat->mChannels ) );
			}

			InsDat->mMutex.unlock();
		}
	}
}

void AudioMixerNode::pinUnlinked( QSharedPointer<fugio::PinInterface> pPinSrc, QSharedPointer<fugio::PinInterface> pPinDst )
{
	if( pPinSrc->direction() != PIN_INPUT )
	{
		return;
	}

	fugio::AudioProducerInterface		*IAP = pPinDst->hasControl() ? qobject_cast<fugio::AudioProducerInterface *>( pPinDst->control()->qobject() ) : nullptr;

	if( IAP )
	{
		for( AudioInstanceData *InsDat : mInstanceData )
		{
			InsDat->mMutex.lock();

			QMap<fugio::AudioProducerInterface *,void *>::iterator	it = InsDat->mInstanceData.find( IAP );

			if( it != InsDat->mInstanceData.end() )
			{
				it.key()->audioFreeInstance( it.value() );

				InsDat->mInstanceData.remove( it.key() );
			}

			InsDat->mMutex.unlock();
		}
	}
}

int AudioMixerNode::audioChannels() const
{
	int			Channels = 0;

	for( AudioInstanceData *InsDat : mInstanceData )
	{
		Channels = std::max( Channels, InsDat->mChannels );
	}

	return( Channels );
}

qreal AudioMixerNode::audioSampleRate() const
{
	return( 48000 );
}

fugio::AudioSampleFormat AudioMixerNode::audioSampleFormat() const
{
	return( fugio::AudioSampleFormat::Format32FS );
}

qint64 AudioMixerNode::audioLatency() const
{
	qint64			Latency = 0;

	for( QSharedPointer<fugio::PinInterface> P : mNode->enumInputPins() )
	{
		fugio::AudioProducerInterface *IAP = input<fugio::AudioProducerInterface *>( P );

		if( IAP )
		{
			Latency = std::max<qint64>( Latency, IAP->audioLatency() );
		}
	}

	return( Latency );
}
