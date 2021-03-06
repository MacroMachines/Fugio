#ifndef DILATENODE_H
#define DILATENODE_H

#include <QObject>

#include <fugio/core/uuid.h>
#include <fugio/opencv/uuid.h>
#include <fugio/node_interface.h>
#include <fugio/node_control_interface.h>
#include <fugio/image/image_interface.h>

#include <fugio/nodecontrolbase.h>

#if defined( OPENCV_SUPPORTED )
#include <opencv2/core/core.hpp>
#endif

class DilateNode : public fugio::NodeControlBase
{
	Q_OBJECT

public:
	Q_INVOKABLE explicit DilateNode( QSharedPointer<fugio::NodeInterface> pNode );

	virtual ~DilateNode( void ) {}

	//-------------------------------------------------------------------------
	// NodeControlInterface

	virtual void inputsUpdated( qint64 pTimeStamp );

private:
	static void conversion( DilateNode *pNode );

private:
	QSharedPointer<fugio::PinInterface>			 mPinInputImage;

	QSharedPointer<fugio::PinInterface>			 mPinOutputImage;
	fugio::ImageInterface								*mOutputImage;

#if defined( OPENCV_SUPPORTED )
	cv::Mat										 mMatImg;
#endif
};

#endif // DILATENODE_H
