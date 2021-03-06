#include "nodenamedialog.h"
#include "ui_nodenamedialog.h"

#include <QDebug>
#include <QSettings>

#include <fugio/fugio.h>
#include <fugio/global_interface.h>

#include "app.h"

#include <fugio/utils.h>

const static int NODE_HISTORY_COUNT = 10;

NodeNameDialog::NodeNameDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::NodeNameDialog)
{
	ui->setupUi(this);

	mNodeList.clear();

	QSettings				 Settings;

	int		HistoryCount = Settings.beginReadArray( "node-history" );

	for( int i = 0 ; i < qMin( NODE_HISTORY_COUNT, HistoryCount ) ; i++ )
	{
		Settings.setArrayIndex( i );

		QUuid NodeUuid = fugio::utils::string2uuid( Settings.value( "uuid" ).toString() );

		mNodeList << NodeUuid;
	}

	Settings.endArray();

	while( mNodeList.size() < NODE_HISTORY_COUNT )
	{
		mNodeList << QUuid();
	}

	setListHistory();
}

NodeNameDialog::~NodeNameDialog()
{
	QSettings				 Settings;

	Settings.beginWriteArray( "node-history" );

	for( int i = 0 ; i < NODE_HISTORY_COUNT ; i++ )
	{
		Settings.setArrayIndex( i );

		Settings.setValue( "uuid", fugio::utils::uuid2string( mNodeList.at( i ) ) );
	}

	Settings.endArray();

	delete ui;
}

void NodeNameDialog::on_lineEdit_textChanged( const QString &arg1 )
{
	if( arg1.isEmpty() )
	{
		setListHistory();
	}
	else
	{
		setListCompare( arg1 );
	}
}

void NodeNameDialog::setListHistory()
{
	ui->listWidget->clear();

	for( int i = 0 ; i < NODE_HISTORY_COUNT ; i++ )
	{
		QUuid		NodeUuid = mNodeList.at( i );

		if( !NodeUuid.isNull() && gApp->global().nodeMap().contains( NodeUuid ) )
		{
			const fugio::ClassEntry	&NodeData = gApp->global().nodeMap().value( NodeUuid );

			if( !NodeData.mFlags.testFlag( fugio::ClassEntry::Deprecated ) )
			{
				QListWidgetItem		*NodeItem = new QListWidgetItem( NodeData.friendlyName() );

				NodeItem->setData( Qt::UserRole, NodeData.mUuid );

				ui->listWidget->addItem( NodeItem );
			}
		}
	}
}

void NodeNameDialog::setListCompare( const QString &pCmp )
{
	const GlobalPrivate::UuidClassEntryMap	&NodeList = gApp->global().nodeMap();

	ui->listWidget->clear();

	for( const fugio::ClassEntry &NodeEntry : NodeList.values() )
	{
		if( NodeEntry.mFlags.testFlag( fugio::ClassEntry::Deprecated ) )
		{
			continue;
		}

		QString		LocalName = NodeEntry.friendlyName();

		if( LocalName.toLower().contains( pCmp.toLower() ) )
		{
			QListWidgetItem		*NodeItem = new QListWidgetItem( LocalName );

			NodeItem->setData( Qt::UserRole, NodeEntry.mUuid );

			ui->listWidget->addItem( NodeItem );
		}
	}
}

void NodeNameDialog::addToHistory( const QUuid &pNodeUuid )
{
	mNodeList.removeAll( pNodeUuid );

	mNodeList.prepend( pNodeUuid );

	while( mNodeList.size() < NODE_HISTORY_COUNT )
	{
		mNodeList << QUuid();
	}

	while( mNodeList.size() > NODE_HISTORY_COUNT )
	{
		mNodeList.pop_back();
	}
}

void NodeNameDialog::on_listWidget_doubleClicked( const QModelIndex &index )
{
	mNodeUuid = index.data( Qt::UserRole ).toUuid();

	if( !mNodeUuid.isNull() )
	{
		addToHistory( mNodeUuid );

		accept();
	}
}

void NodeNameDialog::on_lineEdit_returnPressed()
{
	if( ui->lineEdit->text().isEmpty() )
	{
		mNodeUuid = mNodeList.first();

		if( !mNodeUuid.isNull() )
		{
			accept();
		}
	}
	else if( ui->listWidget->count() > 0 )
	{
		mNodeUuid = ui->listWidget->item( 0 )->data( Qt::UserRole ).toUuid();

		addToHistory( mNodeUuid );

		accept();
	}
}

void NodeNameDialog::on_listWidget_itemActivated( QListWidgetItem *item )
{
	mNodeUuid = item->data( Qt::UserRole ).toUuid();

	if( !mNodeUuid.isNull() )
	{
		addToHistory( mNodeUuid );

		accept();
	}
}
