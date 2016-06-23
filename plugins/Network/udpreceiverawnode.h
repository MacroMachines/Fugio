#ifndef UDPRECEIVERAWNODE_H
#define UDPRECEIVERAWNODE_H

#include <fugio/nodecontrolbase.h>
#include <fugio/core/variant_interface.h>
#include <QUdpSocket>

class UDPReceiveRawNode : public fugio::NodeControlBase
{
	Q_OBJECT
	Q_CLASSINFO( "Author", "Alex May" )
	Q_CLASSINFO( "Version", "1.0" )
	Q_CLASSINFO( "Description", "UDP Server" )
	Q_CLASSINFO( "URL", "http://wiki.bigfug.com/UDP_Receive_Raw" )
	Q_CLASSINFO( "Contact", "http://www.bigfug.com/contact/" )

public:
	Q_INVOKABLE UDPReceiveRawNode( QSharedPointer<fugio::NodeInterface> pNode);

	virtual ~UDPReceiveRawNode( void ) {}

	// NodeControlInterface interface

	virtual void inputsUpdated( qint64 pTimeStamp ) Q_DECL_OVERRIDE;

	virtual bool initialise() Q_DECL_OVERRIDE;
	virtual bool deinitialise() Q_DECL_OVERRIDE;

protected slots:
	void frameStart( qint64 pTimeStamp );

protected:
	QSharedPointer<fugio::PinInterface>			 mPinPort;

	QSharedPointer<fugio::PinInterface>			 mPinOutputBuffer;
	fugio::VariantInterface						*mValOutputBuffer;

	QUdpSocket									 mSocket;

	QDataStream									*mStream;
};

#endif // UDPRECEIVERAWNODE_H