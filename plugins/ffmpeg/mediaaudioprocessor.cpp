#include "mediaaudioprocessor.h"
#include <QDebug>
#include <QFileInfo>
#include <QDateTime>

static const qreal		AUD_INC = 250.0; //44100.0 / 4.41;

MediaAudioProcessor::MediaAudioProcessor( void )
{
	mOptions = Calculate;

#if defined( FFMPEG_SUPPORTED )
	mFormatContext = 0;
	mAudioCodecContext = 0;
	mAudioCodec = 0;
	mFrameSrc = 0;
	mPacketSize = 0;
#if !defined( TL_USE_LIB_AV )
	mSwrContext = 0;
#endif
	mErrorState = false;
	mDuration = 0;
	mAudioPTS = -1;
	mAudioSamplePTS = -1;
	mPreloaded = false;
	mCalculated = false;
	mAudPts = 0;
	mAudNxt = AUD_INC;
#endif
}

MediaAudioProcessor::~MediaAudioProcessor()
{

}

void MediaAudioProcessor::processAudioFrame()
{
#if defined( FFMPEG_SUPPORTED )
	QMutexLocker		Lock( &mAudDatLck );

#if defined( TL_USE_LIB_AV )
#else
	int linesize    = 0;
	int out_samples = av_rescale_rnd( swr_get_delay( mSwrContext, mAudioCodecContext->sample_rate ) + mFrameSrc->nb_samples, 48000, mAudioCodecContext->sample_rate, AV_ROUND_UP);
	int dst_bufsize = av_samples_get_buffer_size( &linesize, mChannels, out_samples, AV_SAMPLE_FMT_FLTP, 1 );
	int	cur_bufsize = ( mOptions.testFlag( Preload ) && !mPreloaded ? mAudBuf.first().size() : 0 );

	QVector<uint8_t *>	OutPtr;

	OutPtr.resize( mChannels );

	for( int i = 0 ; i < mChannels ; i++ )
	{
		mAudBuf[ i ].resize( cur_bufsize + out_samples );

		OutPtr[ i ] = (uint8_t *)&mAudBuf[ i ][ mAD.mAudSmp ];
	}

	if( mChannelLayout == mAudioCodecContext->channel_layout && mChannels == mAudioCodecContext->channels && mSampleRate == mAudioCodecContext->sample_rate && mSampleFmt == mAudioCodecContext->sample_fmt )
	{
		out_samples = swr_convert( mSwrContext, &OutPtr[ 0 ], out_samples, (const uint8_t **)mFrameSrc->data, mFrameSrc->nb_samples );
		dst_bufsize = av_samples_get_buffer_size( &linesize, mChannels, out_samples, AV_SAMPLE_FMT_FLTP, 1 );

		if( !mCalculated )
		{
			for( int c = 0 ; c < mChannels ; c++ )
			{
				AudPrv	&AP     = mAudPrv[ c ];
				float	*SrcPtr = (float *)OutPtr[ c ];

				for( int i = 0 ; i < out_samples ; i++ )
				{
					float		l = SrcPtr[ i ];

					AP.mAudSm = qMax( AP.mAudSm, powf( l * l, 1./8. ) );

					mAudPts += 1.0;

					if( mAudPts >= mAudNxt )
					{
						AP.mAudMax = qMax( AP.mAudSm, AP.mAudMax );

						AP.mAudWav.append( AP.mAudSm );

						AP.mAudSm  = 0;

						mAudNxt += AUD_INC;
					}
				}
			}
		}

		if( mOptions.testFlag( Preload ) && !mPreloaded )
		{
			mAD.mAudSmp += out_samples;
			mAD.mAudLen += linesize;

			if( mAD.mAudSmp > 48000 )
			{
				int		bufsze;
				int		linsze;

				if( ( bufsze = av_samples_alloc( mAD.mAudDat.data(), &linsze, mChannels, mAD.mAudSmp, AV_SAMPLE_FMT_FLTP, 0 ) ) >= 0 )
				{
					for( int i = 0 ; i < mChannels ; i++ )
					{
						memcpy( mAD.mAudDat[ i ], mAudBuf[ i ].data(), mAD.mAudLen );

						//mAD.mAudDat = mAudBuf.
					}

					mAudDat.append( mAD );
				}

				for( int i = 0 ; i < mChannels ; i++ )
				{
					mAudBuf[ i ].clear();

					mAD.mAudDat[ i ] = 0;
				}

				mAD.mAudPts += mAD.mAudSmp;

				mAD.mAudLen = 0;
				mAD.mAudSmp = 0;
			}
		}
	}
#endif
#endif
}

QString MediaAudioProcessor::av_err(const QString &pHeader, int pErrorCode)
{
#if defined( FFMPEG_SUPPORTED )
	char	errbuf[ AV_ERROR_MAX_STRING_SIZE ];

	av_make_error_string( errbuf, AV_ERROR_MAX_STRING_SIZE, pErrorCode );

	return( QString( "%1: %2" ).arg( pHeader ).arg( QString::fromLatin1( errbuf ) ) );
#else
	return( "" );
#endif
}

void MediaAudioProcessor::clearAudio()
{
#if defined( FFMPEG_SUPPORTED )
	QMutexLocker		Lock( &mAudDatLck );

	mAudDat.clear();

	mPreloaded = false;
#endif
}

bool MediaAudioProcessor::loadMedia( const QString &pFileName )
{
#if defined( FFMPEG_SUPPORTED )
	mFileName = pFileName;

	if( avformat_open_input( &mFormatContext, mFileName.toLatin1().constData(), NULL, NULL ) != 0 )
	{
		qWarning() << "Couldn't avformat_open_input" << mFileName;

		mErrorState = true;

		return( false );
	}

	av_format_inject_global_side_data( mFormatContext );

	if( avformat_find_stream_info( mFormatContext, 0 ) < 0 )
	{
		qWarning() << "Couldn't avformat_find_stream_info" << mFileName;

		mErrorState = true;

		return( false );
	}

	mAudioStream = av_find_best_stream( mFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0 );

	// av_find_best_stream might return an error less than -1

	mAudioStream = qMax( -1, mAudioStream );

	for( int i = 0 ; i < (int)mFormatContext->nb_streams ; i++ )
	{
		if( mAudioStream < 0 && mFormatContext->streams[ i ]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO )
		{
			mAudioStream = i;
		}
	}

	if( mAudioStream == -1 )
	{
		return( false );
	}

	if( ( mFrameSrc = av_frame_alloc() ) == 0 )
	{
		//mErrorState = true;

		return( false );
	}

	//-------------------------------------------------------------------------
	// Open Audio Stream

	if( mAudioStream >= 0 )
	{
		AVStream		*Stream = mFormatContext->streams[ mAudioStream ];

		mAudioCodec = avcodec_find_decoder( Stream->codecpar->codec_id );

		if( !mAudioCodec )
		{
			qWarning() << "Couldn't avcodec_find_decoder for stream";

			return( true );
		}

		mAudioCodecContext = avcodec_alloc_context3( NULL );

		if( !mAudioCodecContext )
		{
			return( false );
		}

		avcodec_parameters_to_context( mAudioCodecContext, Stream->codecpar );

		av_codec_set_pkt_timebase( mAudioCodecContext, Stream->time_base );

		mAudioCodecContext->framerate = Stream->avg_frame_rate;

		if( avcodec_open2( mAudioCodecContext, mAudioCodec, NULL ) < 0 )
		{
			qWarning() << "Couldn't avcodec_open2 for stream";

			return( true );
		}
	}

	//-------------------------------------------------------------------------

	av_init_packet( &mPacket );

	//-------------------------------------------------------------------------

	AVRational	AvTimBas;

	AvTimBas.num = 1;
	AvTimBas.den = AV_TIME_BASE;

	AVRational	TimBas;

	TimBas.num = 1;
	TimBas.den = 1;

	if( mDuration == 0 && mFormatContext->duration != AV_NOPTS_VALUE )
	{
		mDuration = qreal( mFormatContext->duration ) * av_q2d( AvTimBas );
	}

	if( mAudioStream != -1 )
	{
		AVStream		*AudioStream   = mFormatContext->streams[ mAudioStream ];
		AVRational		 AudioTimeBase = AudioStream->time_base;

		if( mDuration == 0 && AudioStream->duration != AV_NOPTS_VALUE )
		{
			mDuration = qreal( AudioStream->duration ) * av_q2d( AudioTimeBase );
		}
	}

	if( mAudioCodecContext )
	{
		//if( mAudioCodecContext->channel_layout != AV_CH_LAYOUT_STEREO || mAudioCodecContext->sample_rate != 48000 || mAudioCodecContext->sample_fmt != AV_SAMPLE_FMT_S16 )
		{
#if defined( TL_USE_LIB_AV )
#else
			mChannelLayout = mAudioCodecContext->channel_layout;
			mChannels      = mAudioCodecContext->channels;
			mSampleRate    = mAudioCodecContext->sample_rate;
			mSampleFmt     = mAudioCodecContext->sample_fmt;

			if( ( mSwrContext = swr_alloc() ) != 0 )
			{
				if( mAudioCodecContext->channel_layout == 0 )
				{
					switch( mAudioCodecContext->channels )
					{
						case 1:
							av_opt_set_int( mSwrContext, "in_channel_layout", AV_CH_LAYOUT_MONO, 0 );
							av_opt_set_int( mSwrContext, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
							break;

						case 2:
							av_opt_set_int( mSwrContext, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0 );
							av_opt_set_int( mSwrContext, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
							break;

						default:
							Q_ASSERT( "CHANNELS" );
							break;
					}
				}
				else
				{
					av_opt_set_int( mSwrContext, "in_channel_layout",  mAudioCodecContext->channel_layout, 0 );
					av_opt_set_int( mSwrContext, "out_channel_layout", mAudioCodecContext->channel_layout, 0 );
				}

				av_opt_set_int( mSwrContext, "in_sample_rate",     mAudioCodecContext->sample_rate,    0);
				av_opt_set_sample_fmt( mSwrContext, "in_sample_fmt",  mAudioCodecContext->sample_fmt,  0);

				av_opt_set_int( mSwrContext, "out_sample_rate",    48000,                 0);
				av_opt_set_sample_fmt( mSwrContext, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);

				mAudPrv.resize( mChannels );

				mAD.mAudDat.resize( mChannels );

				mAudBuf.resize( mChannels );

				int	Error;

				if( ( Error = swr_init( mSwrContext ) ) != 0 )
				{
					swr_free( &mSwrContext );

					return( false );
				}
			}
#endif
		}
	}

	return( true );
#else
	return( false );
#endif
}

qreal MediaAudioProcessor::timeToWaveformIdx( qreal pTimeStamp ) const
{
	return( ( pTimeStamp * 48000.0 ) / AUD_INC );
}

qreal MediaAudioProcessor::wavL( qreal pTimeStamp ) const
{
#if defined( FFMPEG_SUPPORTED )
	Q_ASSERT( pTimeStamp >= 0 );

	QMutexLocker		Lock( &mAudDatLck );

//	if( mAudPreloaded )
//	{
//		return( 0.0 );
//	}

	if( mAudPrv.isEmpty() )
	{
		return( 0.0 );
	}

	const AudPrv	&AP = mAudPrv[ 0 ];

	if( AP.mAudWav.isEmpty() )
	{
		return( 0.0 );
	}

	qint64		WavIdx = qMin( qreal( AP.mAudWav.size() - 1 ), timeToWaveformIdx( pTimeStamp ) );

	Q_ASSERT( WavIdx >= 0 );

	if( AP.mAudWav.size() <= WavIdx )
	{
		return( 0 );
	}

	return( AP.mAudWav.at( WavIdx ) );
#else
	return( 0 );
#endif
}

qreal MediaAudioProcessor::wavR( qreal pTimeStamp ) const
{
#if defined( FFMPEG_SUPPORTED )
	Q_ASSERT( pTimeStamp >= 0 );

	QMutexLocker		Lock( &mAudDatLck );

	if( mAudPrv.size() < 2 )
	{
		return( 0.0 );
	}

	const AudPrv	&AP = mAudPrv[ 1 ];

	if( AP.mAudWav.isEmpty() )
	{
		return( 0.0 );
	}

	qint64		WavIdx = qMin( qreal( AP.mAudWav.size() - 1 ), timeToWaveformIdx( pTimeStamp ) );

	Q_ASSERT( WavIdx >= 0 );

	if( AP.mAudWav.size() <= WavIdx )
	{
		return( 0 );
	}

	return( AP.mAudWav.at( WavIdx ) );
#else
	return( 0 );
#endif
}

void MediaAudioProcessor::mixAudio( qint64 pSamplePosition, qint64 pSampleCount, int pChannelOffset, int pChannelCount, void **pBuffers, float pVol ) const
{
#if defined( FFMPEG_SUPPORTED )
	if( !mAudDatLck.tryLock() )
	{
		return;
	}

//	QMutexLocker		Lock( &mAudDatLck );

	//qDebug() << "addData" << pSamplePosition << pSampleCount;

	qint64				SmpCpy = 0;

	for( const AudioBuffer &AD : mAudDat )
	{
		if( pSamplePosition >= AD.mAudPts + AD.mAudSmp )
		{
			continue;
		}

		if( pSamplePosition + pSampleCount <= AD.mAudPts )
		{
			continue;
		}

		qint64		AudPos = qMax( pSamplePosition, AD.mAudPts );
		qint64		AudLen = qMin( ( AD.mAudPts + AD.mAudSmp ) - AudPos, ( pSamplePosition + pSampleCount ) - AudPos );

		qint64		SrcPos = AudPos - AD.mAudPts;
		qint64		DstPos = AudPos - pSamplePosition;

#if defined( QT_DEBUG )
		Q_ASSERT( SrcPos >= 0 );
		Q_ASSERT( DstPos >= 0 );
		Q_ASSERT( SrcPos < AD.mAudSmp );
		Q_ASSERT( DstPos < pSampleCount );
		Q_ASSERT( SrcPos + AudLen <= AD.mAudSmp );
		Q_ASSERT( DstPos + AudLen <= pSampleCount );
#endif

		//qDebug() << SrcPos << QString( "(%1)" ).arg( AD.mAudSmp ) << "->" << DstPos << AudLen;

		for( int i = 0 ; i < pChannelCount ; i++ )
		{
			if( AD.mAudDat.size() <= pChannelOffset + i )
			{
				continue;
			}

			const float	*Src1 = reinterpret_cast<const float *>( AD.mAudDat[ pChannelOffset + i ] ) + SrcPos;
			float		*Src2 = reinterpret_cast<float *>( pBuffers[ i ] ) + DstPos;

			for( int j = 0 ; j < AudLen ; j++ )
			{
				Src2[ j ] += Src1[ j ] * pVol;
			}
		}

		SmpCpy += AudLen;

		if( SmpCpy == pSampleCount )
		{
			break;
		}
	}

//	if( SmpCpy != pSampleCount )
//	{
//		qDebug() << "addData16:" << pSamplePosition << "-" << SmpCpy << "!=" << pSampleCount;
//	}

	mAudDatLck.unlock();
#endif
}

bool MediaAudioProcessor::preloaded() const
{
#if defined( FFMPEG_SUPPORTED )
	return( mPreloaded );
#else
	return( false );
#endif
}

void MediaAudioProcessor::process( void )
{
#if defined( FFMPEG_SUPPORTED )
	mProcessAborted = false;

	qreal	FileSize = QFileInfo( mFileName ).size();

	if( FileSize <= 0 )
	{
		return;
	}

	qDebug() << "Processing:" << mFileName;

	qreal	FilePos  = 0;

	mAudBuf.reserve( 48000 * sizeof( float ) );

	avformat_seek_file( mFormatContext, -1, 0, 0, 0, 0 );

	if( mAudioCodecContext )
	{
		avcodec_flush_buffers( mAudioCodecContext );
	}

	av_packet_unref( &mPacket );

	mPacketSize = 0;

	//mAD.mAudDat = 0;
	mAD.mAudPts = 0;
	mAD.mAudSmp = 0;
	mAD.mAudLen = 0;

	int			mAudioSendResult;
	int			mAudioRecvResult;

	//qint64		TimeS = QDateTime::currentMSecsSinceEpoch();

	while( !mErrorState && !mProcessAborted )
	{
		int			 ReadError;

		if( ( ReadError = av_read_frame( mFormatContext, &mPacket ) ) < 0 )
		{
			if( ReadError != AVERROR_EOF )
			{
				qWarning() << av_err( "av_read_frame", ReadError );

				mErrorState = true;

				//return;
			}

			if( ( mAudioSendResult = avcodec_send_packet( mAudioCodecContext, NULL ) ) == 0 )
			{
				while( ( mAudioRecvResult = avcodec_receive_frame( mAudioCodecContext, mFrameSrc ) ) == 0 )
				{
					processAudioFrame();
				}
			}

			break;
		}

		FilePos += mPacket.size;

		if( mPacket.stream_index == mAudioStream )
		{
			if( ( mAudioSendResult = avcodec_send_packet( mAudioCodecContext, &mPacket ) ) == 0 )
			{
				while( ( mAudioRecvResult = avcodec_receive_frame( mAudioCodecContext, mFrameSrc ) ) == 0 )
				{
					processAudioFrame();
				}
			}
		}
	}

	av_packet_unref( &mPacket );

	mPacketSize = 0;

	if( !mProcessAborted )
	{
//		while( ( BytesRead = avcodec_decode_audio4( mAudioCodecContext, mFrameSrc, &mFrameFinished, &mPacket ) ) > 0 )
//		{
//			if( mFrameFinished != 0 )
//			{
//				processAudioFrame();
//			}
//		}
	}

	if( mOptions.testFlag( Preload ) && !mPreloaded )
	{
		if( mAD.mAudSmp > 0 )
		{
			int		bufsze;
			int		linsze;

			if( ( bufsze = av_samples_alloc( mAD.mAudDat.data(), &linsze, mChannels, mAD.mAudSmp, AV_SAMPLE_FMT_FLTP, 0 ) ) >= 0 )
			{
				for( int i = 0 ; i < mChannels ; i++ )
				{
					memcpy( mAD.mAudDat[ i ], mAudBuf[ i ].data(), mAD.mAudLen );
				}

				mAudDat.append( mAD );
			}

			for( int i = 0 ; i < mChannels ; i++ )
			{
				mAudBuf[ i ].clear();

				mAD.mAudDat[ i ] = 0;
			}

			mAD.mAudPts += mAD.mAudSmp;

			mAD.mAudLen = 0;
			mAD.mAudSmp = 0;
		}
	}

	for( int i = 0 ; i < mChannels ; i++ )
	{
		mAudBuf[ i ].clear();
	}

	mPreloaded = mOptions.testFlag( Preload );

	for( int c = 0 ; c < mChannels ; c++ )
	{
		AudPrv	&AP = mAudPrv[ c ];

		if( AP.mAudMax > 0.0f && AP.mAudMax < 1.0f )
		{
			for( int i = 0 ; i < AP.mAudWav.size() ; i++ )
			{
				AP.mAudWav[ i ] /= AP.mAudMax;
			}
		}
	}

	mCalculated = true;

	//qint64		TimeE = QDateTime::currentMSecsSinceEpoch();

	//qDebug() << "Audio Process Time:" << TimeE - TimeS;

	qDebug() << "Finished Processing:" << mFileName;
#endif
}

void MediaAudioProcessor::abort()
{
#if defined( FFMPEG_SUPPORTED )
	mProcessAborted = true;
#endif
}
