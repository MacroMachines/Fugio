#include "texturemonitornode.h"

#include <QMainWindow>

#include "openglplugin.h"

#include <fugio/opengl/uuid.h>

#include <fugio/editor_interface.h>

#include <fugio/opengl/texture_interface.h>

TextureMonitorNode::TextureMonitorNode( QSharedPointer<fugio::NodeInterface> pNode )
	: NodeControlBase( pNode ), mDockWidget( nullptr ), mWidget( nullptr ), mDockArea( Qt::BottomDockWidgetArea ),
	  mVBO( QOpenGLBuffer::VertexBuffer )
{
	FUGID( PIN_INPUT_TEXTURE, "9e154e12-bcd8-4ead-95b1-5a59833bcf4e" );

	pinInput( "Texture", PIN_INPUT_TEXTURE );
}

bool TextureMonitorNode::initialise()
{
	if( !NodeControlBase::initialise() )
	{
		return( false );
	}

	fugio::EditorInterface	*EI = qobject_cast<fugio::EditorInterface *>( OpenGLPlugin::instance()->app()->findInterface( IID_EDITOR ) );

	if( !EI )
	{
		return( false );
	}

	if( ( mDockWidget = new QDockWidget( QString( "Texture Monitor: %1" ).arg( mNode->name() ), EI->mainWindow() ) ) == 0 )
	{
		return( false );
	}

	mDockWidget->setObjectName( mNode->uuid().toString() );

	if( ( mWidget = new TextureMonitor() ) != nullptr )
	{
		mWidget->setNode( this );

		mDockWidget->setWidget( mWidget );
	}

	EI->mainWindow()->addDockWidget( mDockArea, mDockWidget );

	return( true );
}

bool TextureMonitorNode::deinitialise()
{
	if( mDockWidget )
	{
		mDockWidget->deleteLater();

		mDockWidget = nullptr;

		mWidget = nullptr;
	}

	return( NodeControlBase::deinitialise() );
}

void TextureMonitorNode::inputsUpdated( qint64 pTimeStamp )
{
	Q_UNUSED( pTimeStamp )

	mWidget->update();
}

QList<QUuid> TextureMonitorNode::pinAddTypesInput() const
{
	static const QList<QUuid> PinLst =
	{
		PID_OPENGL_TEXTURE
	};

	return( PinLst );
}

bool TextureMonitorNode::canAcceptPin(fugio::PinInterface *pPin) const
{
	return( pPin->direction() == PIN_OUTPUT );
}

bool TextureMonitorNode::pinShouldAutoRename(fugio::PinInterface *pPin) const
{
	return( pPin->direction() == PIN_OUTPUT );
}

void TextureMonitorNode::paintGL()
{
	if( mNode->status() != fugio::NodeInterface::Initialised )
	{
		return;
	}

	glViewport( 0, 0, mWidget->width(), mWidget->height() );

	glClearColor( 0, 0, 0, 0 );

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

	OPENGL_PLUGIN_DEBUG;

//	if( !mVAO.isCreated() )
//	{
//		mVAO.create();
//	}

//	if( mVAO.isCreated() )
//	{
//		mVAO.bind();
//	}

//	OPENGL_PLUGIN_DEBUG;

//	if( !mVBO.isCreated() )
//	{
//		float Vertices[] = {
//			-1, -1,
//			-1, 1,
//			1, -1,
//			1, 1
//		};

//		if( mVBO.create() )
//		{
//			mVBO.setUsagePattern( QOpenGLBuffer::StaticDraw );

//			mVBO.bind();

//			mVBO.allocate( Vertices, sizeof( Vertices ) );
//		}
//	}

//	OPENGL_PLUGIN_DEBUG;

	if( !mProgram.isLinked() )
	{
		const QString vertexSource =
				"attribute highp vec2 position;\n"
				"varying mediump vec2 tpos;\n"
				"void main()\n"
				"{\n"
				"	gl_Position = vec4( position, 0.0, 1.0 );\n"
				"	tpos = ( position * 0.5 ) + 0.5;\n"
				"	tpos.y = 1.0 - tpos.y;\n"
				"}\n";

		if( !mProgram.addShaderFromSourceCode( QOpenGLShader::Vertex, vertexSource ) )
		{
			mNode->setStatus( fugio::NodeInterface::Error );

			return;
		}

		const QString fragmentSource =
				"uniform sampler2D tex;\n"
				"varying mediump vec2 tpos;\n"
				"void main()\n"
				"{\n"
				"	gl_FragColor = vec4( texture2D( tex, tpos ).rgb, 1.0 );\n"
				"}\n";

		if( !mProgram.addShaderFromSourceCode( QOpenGLShader::Fragment, fragmentSource ) )
		{
			mNode->setStatus( fugio::NodeInterface::Error );

			return;
		}

		if( !mProgram.link() )
		{
			mNode->setStatus( fugio::NodeInterface::Error );

			return;
		}

	}

	OPENGL_PLUGIN_DEBUG;

	if( !mProgram.isLinked() )
	{
		return;
	}

	if( mProgram.bind() )
	{
		static GLfloat const Vertices[] = {
			-1, -1,
			-1, 1,
			1, -1,
			1, 1
		};

		GLint posAttrib = mProgram.attributeLocation( "position" );

		mProgram.enableAttributeArray( posAttrib );

		mProgram.setAttributeArray( posAttrib, Vertices, 2 );

		OPENGL_PLUGIN_DEBUG;

		QList<fugio::OpenGLTextureInterface *>	TexLst;

		for( QSharedPointer<fugio::PinInterface> P : mNode->enumInputPins() )
		{
			fugio::OpenGLTextureInterface	*T = input<fugio::OpenGLTextureInterface *>( P );

			if( T )
			{
				TexLst << T;
			}
		}

		if( TexLst.isEmpty() )
		{
			return;
		}

		double	tw = 1.0 / double( TexLst.size() );

		for( int i = 0 ; i < TexLst.size() ; i++ )
		{
			fugio::OpenGLTextureInterface *T = TexLst[ i ];

			glViewport( tw * i * mWidget->width() * mWidget->devicePixelRatio(), 0, tw * mWidget->width() * mWidget->devicePixelRatio(), mWidget->height() * mWidget->devicePixelRatio() );

			T->srcBind();

			glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );

			T->release();
		}

		OPENGL_PLUGIN_DEBUG;

		mProgram.disableAttributeArray( posAttrib );

		mProgram.release();
	}
}
