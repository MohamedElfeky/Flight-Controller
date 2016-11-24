#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ahrs.h"
#include <QCoreApplication>
#include <QTextStream>
#include <QWebFrame>
#include <QWebElement>
#include "aboutbox.h"
#include "quatutil.h"
#include <math.h>

static char beatString[] = "BEAT";

AHRS ahrs;


const char* graphNames[] = {"GyroX", "GyroY", "GyroZ", "AccelX", "AccelY", "AccelZ", "MagX", "MagY", "MagZ", "GyroTemp",
						   "AltiRaw", "AltiEst", "GroundHeight",
						   "Pitch", "Roll", "Yaw", "Voltage"};
const QColor graphColors[] = { Qt::red, Qt::green, Qt::blue,
								QColor(255,160,160), QColor(160, 255, 160), QColor(160,160,255),
								QColor(255, 96, 96), QColor( 96, 255,  96), QColor( 96, 96,255), QColor(192,64,64),
							   Qt::gray, Qt::black, QColor(255,128,0),
							   QColor(255,0,255), QColor(0,255,255), QColor(160,160,32), QColor(255,0,255)
							 };



MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
    ui(new Ui::MainWindow)
{
	ThrottleCalibrationCycle = 0;
	CalibrateControlsStep = 0;
	CalibrateTimer = 0;
	RadioMode = 2;		// Mode 2 by default - check to see if there's a config file with a different setting
	currentMode = None;

    ui->setupUi(this);

	// Hex configuration is not enabled by default - will get a signal from FC if set
	showHexMode = false;
	ui->btnMotorTest_CR->setVisible(false);
	ui->btnMotorTest_CL->setVisible(false);
	ui->motor_CR_val->setVisible(false);
	ui->motor_CL_val->setVisible(false);

	ui->lblRadioCalibrateDocs->setStyleSheet("QLabel { background-color : orange; color : black; }");

	// These have to match the modes and order defined in Elev8-Main.h in the firmware
	const char * flightModes[] = {"Assisted", "Stable", "Manual", "Auto-Manual", "ReturnHome"};

	for( int i=0; i<(int)(sizeof(flightModes)/sizeof(flightModes[0])); i++ )
	{
		ui->cb_FlightMode_Up->addItem( QString(flightModes[i]) );
		ui->cb_FlightMode_Middle->addItem( QString(flightModes[i]) );
		ui->cb_FlightMode_Down->addItem( QString(flightModes[i]) );
	}

	for( int i=0; i<4; i++ ) {
		accXCal[i] = accYCal[i] = accZCal[i] = 0.f;
	}

	m_sSettingsFile = QApplication::applicationDirPath() + "/GroundStation_settings.ini";
	loadSettings();

    connect( &comm, SIGNAL(connectionMade()), this, SLOT(on_connectionMade()));

    QPixmap iconImage;
    iconImage.load(":/images/groundstation_icon64.png");
    QIcon winIcon;
    winIcon.addPixmap(iconImage);
    this->setWindowIcon(winIcon);


    QString style = QString("QPushButton{\
                            background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 white, stop: 1 lightgrey);\
							border-style: solid; border-color: grey; border-width: 1px; border-radius: 4px;\
                            }\
                            QPushButton:hover{\
                            background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 white, stop: 1 lightblue);\
							border-style: solid; border-color: grey; border-width: 1px; border-radius: 4px;\
                            }\
                            QPushButton:pressed{\
                            background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 lightblue, stop: 1 white);\
							border-style: solid; border-color: grey; border-width: 1px; border-radius: 4px;\
                            }\
                        ");

	// For the motor disable button
	QString style2 = QString("QPushButton{\
						background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 white, stop: 1 lightgrey);\
						border-style: solid; border-color: grey; border-width: 1px; border-radius: 2px;\
						}\
						QPushButton:hover{\
						background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 white, stop: 1 lightblue);\
						border-style: solid; border-color: grey; border-width: 1px; border-radius: 2px;\
						}\
						QPushButton:pressed{\
						background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 white, stop: 1 pink);\
						border-style: solid; border-color: grey; border-width: 1px; border-radius: 2px;\
						}\
						QPushButton:checked{\
						background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #ff8080, stop: 1 #ffd0d0);\
						border-style: solid; border-color: grey; border-width: 1px; border-radius: 2px;\
						}\
					");

	InternalChange = true;

	ui->motor_FR_val->setOrigin(ValueBar_Widget::Left);
	ui->motor_CR_val->setOrigin(ValueBar_Widget::Left);
	ui->motor_BR_val->setOrigin(ValueBar_Widget::Left);

	ui->motor_FL_val->setOrigin(ValueBar_Widget::Right);
	ui->motor_CL_val->setOrigin(ValueBar_Widget::Right);
	ui->motor_BL_val->setOrigin(ValueBar_Widget::Right);

    ui->motor_FL_val->setMinMax( 8000, 16000 );
    ui->motor_FL_val->setLeftLabel( "FL" );
    ui->motor_FL_val->setBarColor( QColor::fromRgb(255,160,128) );

    ui->motor_FR_val->setMinMax( 8000, 16000 );
    ui->motor_FR_val->setRightLabel( "FR" );
    ui->motor_FR_val->setBarColor( QColor::fromRgb(255,160,128) );

	ui->motor_CL_val->setMinMax( 8000, 16000 );
	ui->motor_CL_val->setLeftLabel( "CL" );
	ui->motor_CL_val->setBarColor( QColor::fromRgb(255,160,128) );

	ui->motor_CR_val->setMinMax( 8000, 16000 );
	ui->motor_CR_val->setRightLabel( "CR" );
	ui->motor_CR_val->setBarColor( QColor::fromRgb(255,160,128) );

    ui->motor_BR_val->setMinMax( 8000, 16000 );
    ui->motor_BR_val->setRightLabel( "BR" );
    ui->motor_BR_val->setBarColor( QColor::fromRgb(255,160,128) );

    ui->motor_BL_val->setMinMax( 8000, 16000 );
    ui->motor_BL_val->setLeftLabel( "BL" );
    ui->motor_BL_val->setBarColor( QColor::fromRgb(255,160,128) );

    ui->btnMotorTest_FL->setStyleSheet( style );
    ui->btnMotorTest_FR->setStyleSheet( style );
	ui->btnMotorTest_CL->setStyleSheet( style );
	ui->btnMotorTest_CR->setStyleSheet( style );
	ui->btnMotorTest_BL->setStyleSheet( style );
    ui->btnMotorTest_BR->setStyleSheet( style );
    ui->btnBeeper->setStyleSheet( style );
    ui->btnLED->setStyleSheet( style );
    ui->btnThrottleCalibrate->setStyleSheet( style );

	ui->btnDisableMotors->setStyleSheet(style2);

    ui->rollPowerVal->setLeftLabel( "Roll Power" );
    ui->pitchPowerVal->setLeftLabel( "Pitch Power" );
    ui->yawPowerVal->setLeftLabel( "Yaw Power" );

    ui->batteryVal->setLeftLabel( "Battery Voltage" );
    ui->batteryVal->setMinMax( 900, 1260 );

	ui->channel1Val->setLeftLabel("Thro");
	ui->channel2Val->setLeftLabel("Aile");
	ui->channel3Val->setLeftLabel("Elev");
	ui->channel4Val->setLeftLabel("Rudd");
    ui->channel5Val->setLeftLabel("Gear");
    ui->channel6Val->setLeftLabel("Aux1");
    ui->channel7Val->setLeftLabel("Aux2");
	ui->channel8Val->setLeftLabel("Aux3");
	ui->vbR_Channel1->setLeftLabel("Thro");
	ui->vbR_Channel2->setLeftLabel("Aile");
	ui->vbR_Channel3->setLeftLabel("Elev");
	ui->vbR_Channel4->setLeftLabel("Rudd");
    ui->vbR_Channel5->setLeftLabel("Gear");
    ui->vbR_Channel6->setLeftLabel("Aux1");
    ui->vbR_Channel7->setLeftLabel("Aux2");
    ui->vbR_Channel8->setLeftLabel("Aux3");

	ui->channel1Val->setOrigin(ValueBar_Widget::Center);
	ui->channel2Val->setOrigin(ValueBar_Widget::Center);
	ui->channel3Val->setOrigin(ValueBar_Widget::Center);
	ui->channel4Val->setOrigin(ValueBar_Widget::Center);
	ui->channel5Val->setOrigin(ValueBar_Widget::Center);
	ui->channel6Val->setOrigin(ValueBar_Widget::Center);
	ui->channel7Val->setOrigin(ValueBar_Widget::Center);
	ui->channel8Val->setOrigin(ValueBar_Widget::Center);
	ui->vbR_Channel1->setOrigin(ValueBar_Widget::Center);
	ui->vbR_Channel2->setOrigin(ValueBar_Widget::Center);
	ui->vbR_Channel3->setOrigin(ValueBar_Widget::Center);
	ui->vbR_Channel4->setOrigin(ValueBar_Widget::Center);
	ui->vbR_Channel5->setOrigin(ValueBar_Widget::Center);
	ui->vbR_Channel6->setOrigin(ValueBar_Widget::Center);
	ui->vbR_Channel7->setOrigin(ValueBar_Widget::Center);
	ui->vbR_Channel8->setOrigin(ValueBar_Widget::Center);

	ui->rollPowerVal->setOrigin(ValueBar_Widget::Center);
	ui->pitchPowerVal->setOrigin(ValueBar_Widget::Center);
	ui->yawPowerVal->setOrigin(ValueBar_Widget::Center);

	FillChannelComboBox( ui->cbR_Channel1, RadioMode == 2 ? 1 : 3 );
    FillChannelComboBox( ui->cbR_Channel2, 2 );
	FillChannelComboBox( ui->cbR_Channel3, RadioMode == 2 ? 3 : 1 );
    FillChannelComboBox( ui->cbR_Channel4, 4 );
    FillChannelComboBox( ui->cbR_Channel5, 5 );
    FillChannelComboBox( ui->cbR_Channel6, 6 );
    FillChannelComboBox( ui->cbR_Channel7, 7 );
    FillChannelComboBox( ui->cbR_Channel8, 8 );

    ui->rollPowerVal->setMinMax( -5000, 5000 );
    ui->rollPowerVal->setBarColor( QColor::fromRgb(128,255,255) );
    ui->pitchPowerVal->setMinMax( -5000, 5000 );
    ui->pitchPowerVal->setBarColor( QColor::fromRgb(128,255,255) );
    ui->yawPowerVal->setMinMax( -5000, 5000 );
    ui->yawPowerVal->setBarColor( QColor::fromRgb(128,255,255) );


    ui->cbReceiverType->addItem(QString("PWM"));
    ui->cbReceiverType->addItem(QString("S-Bus"));
    ui->cbReceiverType->addItem(QString("PPM"));
	ui->cbReceiverType->addItem(QString("RemoteRX/SRLX"));

	ui->cbArmingDelay->addItem(QString("1.00 sec"));
	ui->cbArmingDelay->addItem(QString("0.50 sec"));
	ui->cbArmingDelay->addItem(QString("0.25 sec"));
	ui->cbArmingDelay->addItem(QString("Off"));

	ui->cbDisarmDelay->addItem(QString("1.00 sec"));
	ui->cbDisarmDelay->addItem(QString("0.50 sec"));
	ui->cbDisarmDelay->addItem(QString("0.25 sec"));
	ui->cbDisarmDelay->addItem(QString("Off"));

	ui->vbVoltage2->setLeftLabel("Battery Voltage");
	ui->vbVoltage2->setMinMax( 900, 1260 );

	ui->gCalibTemp->setRange(8192);
	ui->gCalibX->setRange(8192);
	ui->gCalibY->setRange(8192);
	ui->gCalibZ->setRange(8192);

	ui->gAccelXCal->setRange(32768);
	ui->gAccelYCal->setRange(32768);
	ui->gAccelZCal->setRange(32768);

	SetRadioMode(RadioMode);

	labelStatus = new QLabel(this);
	labelGSVersion = new QLabel(this);
	labelFWVersion = new QLabel(this);

    // set text for the label
	labelStatus->setText("Connecting...");
	labelStatus->setContentsMargins( 5, 1, 5, 1 );
	labelGSVersion->setText("GroundStation Version 3.0.0");
	labelFWVersion->setText( "Firmware Version -.-.-");

	// add the controls to the status bar
	ui->statusBar->addPermanentWidget(labelStatus, 1);
	ui->statusBar->addPermanentWidget(labelGSVersion, 1);
	ui->statusBar->addPermanentWidget(labelFWVersion, 1);

	ui->statusBar->setStyleSheet( "QStatusBar::item { border: 0px solid black }; ");

	labelStatus->setFrameStyle(QFrame::NoFrame);
	labelGSVersion->setFrameStyle(QFrame::NoFrame);
	labelFWVersion->setFrameStyle(QFrame::NoFrame);

	AdjustFonts();

	SampleIndex = 0;
	altiAxisOffset = 0;

	sg = ui->sensorGraph;
	sg->legend->setVisible(true);
	sg->setAutoAddPlottableToLegend(true);
	sg->xAxis->setRange(0, 2000);
	sg->yAxis->setRange(-2048, 2048);

	sg->yAxis2->setVisible(true);
	sg->yAxis2->setRange(-2048, 2048);

	for( int i=0; i<17; i++ )
	{
		if( strcmp(graphNames[i], "AltiRaw") == 0 || strcmp(graphNames[i], "AltiEst") == 0 ) {
			graphs[i] = sg->addGraph( sg->xAxis, sg->yAxis2 );
		}
		else {
			graphs[i] = sg->addGraph();
		}

		graphs[i]->setName(graphNames[i]);
		graphs[i]->setPen( QPen(graphColors[i]) );
	}

	sg->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);

	mapReady = false;
	QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
	ui->webView->setUrl( QUrl("qrc:/google_maps.html") );

	InternalChange = false;
	this->startTimer(25, Qt::PreciseTimer);		// 40 updates / sec
	comm.StartConnection();

    Heartbeat = 0;
}


void MainWindow::on_webView_loadFinished(bool)
{
	mapReady = true;
}


MainWindow::~MainWindow()
{
    comm.StopConnection();
    delete ui;
}


void MainWindow::resizeEvent(QResizeEvent *evt)
{
	QMainWindow::resizeEvent(evt);
	AdjustFonts();
}


void MainWindow::AdjustFonts(void)
{
	int minSize = (float) qMin(this->width(), this->height()*3/2);

#ifdef __APPLE__
    int pixSize = 1 + (minSize / 75);
#else
    int pixSize = 2 + (minSize / 75);
#endif

	QFont smallFont = ui->btnReceiverReset->font();

	int oldPixSize = smallFont.pixelSize();
	if( oldPixSize == pixSize ) return;

	smallFont.setPixelSize(pixSize);

	// Do this here because Mac font defaults are not the same point size as PC
	ui->tabWidget->setFont(smallFont);
	ui->menuBar->setFont(smallFont);
	ui->lblCycles->setFont(smallFont);

	ui->btnBeeper->setFont(smallFont);
	ui->btnLED->setFont(smallFont);
	ui->btnThrottleCalibrate->setFont(smallFont);
	ui->btnMotorTest_FL->setFont(smallFont);
	ui->btnMotorTest_FR->setFont(smallFont);
	ui->btnMotorTest_BL->setFont(smallFont);
	ui->btnMotorTest_BR->setFont(smallFont);
	ui->btnMotorTest_CL->setFont(smallFont);
	ui->btnMotorTest_CR->setFont(smallFont);

	ui->label_3->setFont(smallFont);
	ui->label_4->setFont(smallFont);
	ui->label_8->setFont(smallFont);
	ui->label_9->setFont(smallFont);
	ui->lblAutoRollPitchSpeed->setFont(smallFont);
	ui->lblAutoYawSpeed->setFont(smallFont);
	ui->lblManualRollPitchSpeed->setFont(smallFont);
	ui->lblManualYawSpeed->setFont(smallFont);

	ui->label_46->setFont(smallFont);
	ui->label_47->setFont(smallFont);
	ui->lblPitchRollExpo->setFont(smallFont);
	ui->lblYawExpo->setFont(smallFont);

	ui->label_11->setFont(smallFont);
	ui->label_37->setFont(smallFont);
	ui->label_10->setFont(smallFont);
	ui->lblAccelCorrection->setFont(smallFont);
	ui->lblAccelCorrectionFilter->setFont(smallFont);
	ui->lblThrustCorrection->setFont(smallFont);

	ui->btnReceiverReset->setFont(smallFont);
	ui->btnReceiverCalibrate->setFont(smallFont);
	ui->label_2->setFont(smallFont);
	ui->cbReceiverType->setFont(smallFont);
	ui->cbR_Channel1->setFont(smallFont);
	ui->cbR_Channel2->setFont(smallFont);
	ui->cbR_Channel3->setFont(smallFont);
	ui->cbR_Channel4->setFont(smallFont);
	ui->cbR_Channel5->setFont(smallFont);
	ui->cbR_Channel6->setFont(smallFont);
	ui->cbR_Channel7->setFont(smallFont);
	ui->cbR_Channel8->setFont(smallFont);
	ui->btnUploadRadioChanges->setFont(smallFont);
	ui->btnDisableMotors->setFont(smallFont);
	ui->btnUploadSystemSetup->setFont(smallFont);
	ui->lblRadioCalibrateDocs->setFont(smallFont);

	ui->lblGyroSampleQuality->setFont(smallFont);
	ui->lblGyroSampleRange->setFont(smallFont);

	labelStatus->setFont(smallFont);
	labelGSVersion->setFont(smallFont);
	labelFWVersion->setFont(smallFont);
}


void MainWindow::loadSettings(void)
{
	QSettings settings(m_sSettingsFile, QSettings::IniFormat);

	QVariant radioVar( RadioMode );
	QVariant rMode = settings.value("RadioMode", radioVar );

	RadioMode = rMode.toInt();
}

void MainWindow::saveSettings(void)
{
	QSettings settings(m_sSettingsFile, QSettings::IniFormat);
	settings.setValue( "RadioMode", RadioMode );
}


void MainWindow::on_actionReset_Flight_Controller_triggered()
{
	comm.Reset();
}

void MainWindow::on_actionRadio_Mode_1_triggered() {
	SetRadioMode(1);
}

void MainWindow::on_actionRadio_Mode_2_triggered() {
	SetRadioMode(2);
}

void MainWindow::on_actionRestore_Factory_Defaults_triggered()
{
	SendCommand( "WIPE" );	// Reset prefs
	SendCommand( "QPRF" );	// Query prefs (forces to be applied to UI)
}



void MainWindow::FillChannelComboBox(QComboBox *cb, int defaultIndex )
{
	cb->clear();
    for( int i=0; i<8; i++)
    {
        QString str = QString("Ch %1").arg(i+1);
        if( (i+1) == defaultIndex ) {
            str.append('*');
        }
        QVariant var(i);
        cb->addItem( str, var );
    }
    cb->setCurrentIndex( defaultIndex-1 );
}

void MainWindow::on_connectionMade()
{
	SendCommand( "QPRF" );
}

void MainWindow::timerEvent(QTimerEvent * e)
{
	(void)e;	// prevent unused parameter warning
	UpdateStatus();

	Heartbeat++;
	if( Heartbeat >= 20 && ThrottleCalibrationCycle == 0 )	// don't send heartbeat during throttle calibration
	{
		Heartbeat = 0;
		SendCommand( beatString );	// Send the connection heartbeat
	}

	ProcessPackets();
	CheckCalibrateControls();
}


void MainWindow::UpdateStatus(void)
{
    CommStatus newStat = comm.Status();
    if(newStat != stat)
    {
        stat = newStat;
        switch( stat )
        {
        case CS_Initializing:
			labelStatus->setText("Initializing");
            break;

        case CS_NoDevice:
			labelStatus->setText("No serial devices found");
            break;

        case CS_NoElev8:
			labelStatus->setText("No Elev-8 found");
            break;

        case CS_Connected:
			labelStatus->setText("Connected");
            break;
        }
    }
}


void MainWindow::SendCommand( const char * command )
{
    comm.Send( (quint8*)command, 4 );
}

void MainWindow::SendCommand( QString command )
{
    QByteArray arr = command.toLocal8Bit();
    comm.Send( (quint8*)arr.constData(), arr.length() );
}

void MainWindow::SetRadioMode(int mode)
{
	if( mode != 1 && mode != 2 ) return;

	RadioMode = mode;
	ui->actionRadio_Mode_1->setChecked( mode == 1 );
	ui->actionRadio_Mode_2->setChecked( mode == 2 );

	bool bCacheInternal = InternalChange;
	InternalChange = true;
	FillChannelComboBox( ui->cbR_Channel1, RadioMode == 2 ? 1 : 3 );
	FillChannelComboBox( ui->cbR_Channel3, RadioMode == 2 ? 3 : 1 );

	if(RadioMode == 1)
	{
		ui->channel1Val->setLeftLabel("Elev");
		ui->channel3Val->setLeftLabel("Thro");
		ui->vbR_Channel1->setLeftLabel("Elev");
		ui->vbR_Channel3->setLeftLabel("Thro");

		ui->cbR_Channel1->setCurrentIndex( prefs.ElevChannel );
		ui->cbR_Channel3->setCurrentIndex( prefs.ThroChannel );
	}
	else if(RadioMode == 2)
	{
		ui->channel1Val->setLeftLabel("Thro");
		ui->channel3Val->setLeftLabel("Elev");
		ui->vbR_Channel1->setLeftLabel("Thro");
		ui->vbR_Channel3->setLeftLabel("Elev");

		ui->cbR_Channel1->setCurrentIndex( prefs.ThroChannel );
		ui->cbR_Channel3->setCurrentIndex( prefs.ElevChannel );
	}
	InternalChange = bCacheInternal;

	if( InternalChange ) return;

	// Save radio mode to prefs file
	saveSettings();
}


void MainWindow::AddGraphSample(int GraphIndex, float SampleValue)
{
	//graphs[GraphIndex]->removeDataBefore( SampleIndex );
	//graphs[GraphIndex]->removeData( SampleIndex );
	graphs[GraphIndex]->addData( SampleIndex, SampleValue );
}



//-----------------------------------------------------------------------
static const unsigned int BRAD_PI_SHIFT=14,    BRAD_PI = 1<<BRAD_PI_SHIFT;
static const unsigned int BRAD_HPI= BRAD_PI/2, BRAD_2PI= BRAD_PI*2;
static const unsigned int ATAN_ONE = 0x1000,   ATAN_FP = 12;


// Get the octant a coordinate pair is in.
#define OCTANTIFY(_x, _y, _o)   {                               \
	int _t; _o= 0;                                              \
	if(_y<  0)  {            _x= -_x;   _y= -_y; _o += 4; }     \
	if(_x<= 0)  { _t= _x;    _x=  _y;   _y= -_t; _o += 2; }     \
	if(_x<=_y)  { _t= _y-_x; _x= _x+_y; _y=  _t; _o += 1; }     \
  }

static unsigned short atan2Cordic(int x, int y)
{
	if(y==0)    return (x>=0 ? 0 : BRAD_PI);

	int phi;

	OCTANTIFY(x, y, phi);
	phi *= BRAD_PI/4;

	// Scale up a bit for greater accuracy.
	if(x < 0x10000)
	{
		x *= 0x1000;
		y *= 0x1000;
	}

	// atan(2^-i) terms using PI=0x10000 for accuracy
	const unsigned short list[]=
	{
		0x4000, 0x25C8, 0x13F6, 0x0A22, 0x0516, 0x028C, 0x0146, 0x00A3,
		0x0051, 0x0029, 0x0014, 0x000A, 0x0005, 0x0003, 0x0001, 0x0001
	};

	int i, tmp, dphi=0;
	for(i=1; i<12; i++)
	{
		if(y>=0)
		{
			tmp= x + (y>>i);
			y  = y - (x>>i);
			x  = tmp;
			dphi += list[i];
		}
		else
		{
			tmp= x - (y>>i);
			y  = y + (x>>i);
			x  = tmp;
			dphi -= list[i];
		}
	}
	return phi + (dphi>>2);
}
//-----------------------------------------------------------------------


void TestATan(void)
{
	static int done = 1;
	if( done ) return;

	for( int a=0; a<32768; a+=2 )
	{
		double ang = (double)a / 32768.0 * 3.141592654 * 2.0;

		int sina = (int)(sin(ang) * 4096.0);
		int cosa = (int)(cos(ang) * 4096.0);

		int x = atan2Cordic(cosa, sina);

		qDebug() << QString("a: %1, x: %2,  diff: %3").arg(a).arg(x).arg(a - x);
	}
	done = 1;
}


const float PI = 3.141592654f;

void MainWindow::ProcessPackets(void)
{
    bool bRadioChanged = false;
    bool bDebugChanged = false;
	bool bSensorsChanged = false;
    bool bQuatChanged = false;
    bool bTargetQuatChanged = false;
    bool bMotorsChanged = false;
    bool bComputedChanged = false;
    bool bPrefsChanged = false;

	TestATan();

    packet * p;
    do {
        p = comm.GetPacket();
        if(p != 0)
        {
            switch( p->mode )
            {
                case 1:	// Radio data
					packedRadio.ReadFrom( p );
					radio = packedRadio;

					AddGraphSample( 16, (float)radio.BatteryVolts );
					bRadioChanged = true;
                    break;

                case 2:	// Sensor values
                    sensors.ReadFrom( p );

					AddGraphSample( 0, sensors.GyroX );
					AddGraphSample( 1, sensors.GyroY );
					AddGraphSample( 2, sensors.GyroZ );
					AddGraphSample( 3, sensors.AccelX );
					AddGraphSample( 4, sensors.AccelY );
					AddGraphSample( 5, sensors.AccelZ );
					AddGraphSample( 6, sensors.MagX );
					AddGraphSample( 7, sensors.MagY );
					AddGraphSample( 8, sensors.MagZ );
					AddGraphSample( 9, sensors.Temp );

					ahrs.Update( sensors , (1.0/250.f) * 8.0f , false );

					bSensorsChanged = true;
                    break;

                case 3:	// Quaternion
                    {
					q.setX(      p->GetFloat() );
					q.setY(      p->GetFloat() );
					q.setZ(      p->GetFloat() );
					q.setScalar( p->GetFloat() );

					// TEST CODE
					//q = ahrs.quat;

					QMatrix3x3 m;
					m = QuatToMatrix( q );

					double roll = asin(  m(1, 0) ) * (180.0 / PI) * 100.0;
					double pitch = asin( m(1, 2) ) * (180.0 / PI) * 100.0;
					double yaw = -atan2( m(2, 0), m(2, 2) ) * (180.0 / PI) * 100.0;

					AddGraphSample( 13, (float)pitch );
					AddGraphSample( 14, (float)roll );
					AddGraphSample( 15, (float)yaw );

                    bQuatChanged = true;
                    }
                    break;

                case 4:	// Compute values
                    computed.ReadFrom( p );
                    bComputedChanged = true;

					AddGraphSample( 10, (float)computed.Alt );
					AddGraphSample( 11, (float)computed.AltiEst );
					AddGraphSample( 12, (float)computed.GroundHeight );

					if( SampleIndex < 4 ) {
						altiAxisOffset = computed.AltiEst;
						ui->sensorGraph->yAxis2->setRange(-2048 + altiAxisOffset , 2048 + altiAxisOffset );
					}
					else {
						QCPRange range = sg->yAxis->range();
						ui->sensorGraph->yAxis2->setRange( range.lower + altiAxisOffset , range.upper + altiAxisOffset );
					}
					break;

                case 5:	// Motor values
                    motors.ReadFrom( p );
                    bMotorsChanged = true;
                    break;

                case 6:	// Control quaternion
					cq.setX      ( p->GetFloat() );
					cq.setY      ( p->GetFloat() );
					cq.setZ      ( p->GetFloat() );
					cq.setScalar ( p->GetFloat() );
                    bTargetQuatChanged = true;

					// this is actually the last packet sent by the quad, so use this to advance the sample index
					SampleIndex++;
					break;


                case 7:	// Debug data
                    debugData.ReadFrom( p );
                    bDebugChanged = true;
                    break;

				case 8:	// GPS data
					gpsData.ReadFrom( p );
					{
						static bool mapCoordSet = false;
						static int updateCounter = 0;

						double lat = (double)gpsData.Latitude / 10000000.0;
						double lon = (double)gpsData.Longitude / 10000000.0;

						double homeLat = (double)gpsData.TargetLat / 10000000.0;
						double homeLon = (double)gpsData.TargetLong / 10000000.0;

						if( mapReady == true && (gpsData.SatCount > 4) )
						{
							if( mapCoordSet == false )
							{
								QString str =
									QString("var newLoc = new google.maps.LatLng(%1, %2); ").arg(lat).arg(lon) +
									QString("map.setCenter(newLoc);") +

									QString("loc = new google.maps.Marker({") +
									QString("position: new google.maps.LatLng(%1, %2),").arg(lat).arg(lon) +
									QString("map: map,") +
									QString("title: \"E8\",") +
									QString("label: \"E8\"") +
									QString("});") +
									QString("markers.push(loc);") +

									QString("home = new google.maps.Marker({") +
									QString("position: new google.maps.LatLng(%1, %2),").arg(lat).arg(lon) +
									QString("map: map,") +
									QString("title: \"Home\",") +
									QString("label: \"X\"") +
									QString("});") +
									QString("markers.push(home);");

								ui->webView->page()->currentFrame()->documentElement().evaluateJavaScript(str);
								mapCoordSet = true;
							}
							else
							{
								if( updateCounter == 0 ) {
									QString str = QString("loc.setPosition( new google.maps.LatLng(%1, %2) );").arg(lat).arg(lon);
									str += QString("home.setPosition( new google.maps.LatLng(%1, %2) );").arg(homeLat).arg(homeLon);
									if( ui->cbTrackLocation->isChecked() ) {
										str += "map.setCenter( loc.position );";
									}

									ui->webView->page()->currentFrame()->documentElement().evaluateJavaScript(str);

									//ui->lblTargetDist->setText( QString("Dist: %1  (%2,%3)").arg(gpsData.TargetDist)
									//							.arg(gpsData.YawSin).arg(gpsData.YawCos) );

									//int angle = atan2Cordic( gpsData.TargetDirY, gpsData.TargetDirX );
									//angle -= 16384;
									//if( angle > 16384 ) angle -= -32768;
									//ui->gTargetDir->setValue( angle );
								}
								updateCounter = (updateCounter + 1) & 3;
							}
						}

						int sats = gpsData.SatCount;
						float dil = (float)gpsData.Dilution * 0.1f;
						QString stat;
						if( sats == 255 ) {
							stat = "Waiting for power up";
						}
						else if( sats == 0 ) {
							stat = "Waiting for satellite lock";
						}
						else {
							stat = QString("Tracking %1 satellites").arg(sats);
						}
						ui->lblGpsStatus->setText( stat );
						char NS = lat < 0.0 ? 'S' : 'N';
						char EW = lon < 0.0 ? 'W' : 'E';

						if( sats >= 3 ) {
							stat = QString("Lat: %1 %2  Long: %3 %4  Precision: %5")
									.arg(lat,10,'f',7).arg(NS)
									.arg(lon,10,'f',7).arg(EW).arg(dil);
						}
						else {
							stat = "Lat: -.-  Long: -.-";
						}
						ui->lblGpsCoord->setText(stat);
					}

					// With GPS enabled, this is actually the last packet sent by the quad, so use this to advance the sample index
					SampleIndex++;
					break;

                case 0x18:	// Settings
					{
						PREFS tempPrefs;
						memset( &tempPrefs, 0, sizeof(tempPrefs) );

						quint32 toCopy = p->len;
						if( sizeof(tempPrefs) < toCopy )
							toCopy = sizeof(tempPrefs);

						memcpy( &tempPrefs, p->data, toCopy );

						if( Prefs_CalculateChecksum( tempPrefs ) == tempPrefs.Checksum ) {
							//PrefsReceived = true;	// Global indicator of valid prefs
							bPrefsChanged = true;	// local indicator, just to set up the UI
							prefs = tempPrefs;
						}
						else {
							SendCommand( "QPRF" );	// reqeust them again because the checksum failed
						}
					}
					break;
            }
            delete p;
        }
    } while(p != 0);

    if( bRadioChanged )
    {
		QString volts = QString::number((float)radio.BatteryVolts / 100.f, 'f', 2);

		ui->batteryVal->setValue( radio.BatteryVolts );
		ui->batteryVal->setRightLabel( volts );

		ui->vbVoltage2->setValue( radio.BatteryVolts );
		ui->vbVoltage2->setRightLabel( volts );
		ui->lblVoltage->setText( volts );


		if( radio.Gear > 512 )
			ui->vsFlightMode->setValue(0);	// down
		else if( radio.Gear < -512 )
			ui->vsFlightMode->setValue(2);	// up
		else
			ui->vsFlightMode->setValue(1);	// middle


		if( RadioMode == 2 )
		{
			ui->radioLeft->setValues( radio.Rudd, radio.Thro );
			ui->radioRight->setValues( radio.Aile, radio.Elev );
			ui->radioLeft_R->setValues( radio.Rudd, radio.Thro );
			ui->radioRight_R->setValues( radio.Aile, radio.Elev );

			ui->channel1Val->setValue( radio.Thro );
			ui->channel1Val->setRightLabel( radio.Thro );
			ui->vbR_Channel1->setValue( radio.Thro );
			ui->vbR_Channel1->setRightLabel( radio.Thro );

			ui->channel3Val->setValue( radio.Elev );
			ui->channel3Val->setRightLabel( radio.Elev );
			ui->vbR_Channel3->setValue( radio.Elev );
			ui->vbR_Channel3->setRightLabel( radio.Elev );
		}
		else
		{
			ui->radioLeft->setValues( radio.Rudd, radio.Elev );
			ui->radioRight->setValues( radio.Aile, radio.Thro  );
			ui->radioLeft_R->setValues( radio.Rudd, radio.Elev );
			ui->radioRight_R->setValues( radio.Aile, radio.Thro );

			ui->channel1Val->setValue( radio.Elev );
			ui->channel1Val->setRightLabel( radio.Elev );
			ui->vbR_Channel1->setValue( radio.Elev );
			ui->vbR_Channel1->setRightLabel( radio.Elev );

			ui->channel3Val->setValue( radio.Thro );
			ui->channel3Val->setRightLabel( radio.Thro );
			ui->vbR_Channel3->setValue( radio.Thro );
			ui->vbR_Channel3->setRightLabel( radio.Thro );
		}

        ui->channel2Val->setValue( radio.Aile );
        ui->channel2Val->setRightLabel( radio.Aile );
        ui->vbR_Channel2->setValue( radio.Aile );
        ui->vbR_Channel2->setRightLabel( radio.Aile );


        ui->channel4Val->setValue( radio.Rudd );
        ui->channel4Val->setRightLabel( radio.Rudd );
        ui->vbR_Channel4->setValue( radio.Rudd );
        ui->vbR_Channel4->setRightLabel( radio.Rudd );

        ui->channel5Val->setValue( radio.Gear );
        ui->channel5Val->setRightLabel( radio.Gear );
        ui->vbR_Channel5->setValue( radio.Gear );
        ui->vbR_Channel5->setRightLabel( radio.Gear );

        ui->channel6Val->setValue( radio.Aux1 );
        ui->channel6Val->setRightLabel( radio.Aux1 );
        ui->vbR_Channel6->setValue( radio.Aux1 );
        ui->vbR_Channel6->setRightLabel( radio.Aux1 );

        ui->channel7Val->setValue( radio.Aux2 );
        ui->channel7Val->setRightLabel( radio.Aux2 );
        ui->vbR_Channel7->setValue( radio.Aux2 );
        ui->vbR_Channel7->setRightLabel( radio.Aux2 );

        ui->channel8Val->setValue( radio.Aux3 );
        ui->channel8Val->setRightLabel( radio.Aux3 );
        ui->vbR_Channel8->setValue( radio.Aux3 );
        ui->vbR_Channel8->setRightLabel( radio.Aux3 );
    }

    if( bMotorsChanged )
    {
		if( motors.isHex != showHexMode )
		{
			QIcon icon_CW = ui->btnMotorTest_FL->icon();
			QIcon icon_CCW = ui->btnMotorTest_FR->icon();

			if( motors.isHex ) {
				ui->btnMotorTest_CR->setIcon( icon_CW );
				ui->btnMotorTest_BR->setIcon( icon_CCW );
				ui->btnMotorTest_BL->setIcon( icon_CW );
				ui->btnMotorTest_CL->setIcon( icon_CCW );

				ui->btnMotorTest_CR->setVisible(true);
				ui->btnMotorTest_CL->setVisible(true);

				ui->motor_CR_val->setVisible(true);
				ui->motor_CL_val->setVisible(true);
			}
			else {
				ui->btnMotorTest_BR->setIcon( icon_CCW );
				ui->btnMotorTest_BL->setIcon( icon_CW );

				ui->btnMotorTest_CR->setVisible(false);
				ui->btnMotorTest_CL->setVisible(false);

				ui->motor_CR_val->setVisible(false);
				ui->motor_CL_val->setVisible(false);
			}
			showHexMode = motors.isHex;
		}

        ui->motor_FL_val->setValue( motors.FL );
		ui->motor_FL_val->setRightLabel( motors.FL/8 );

        ui->motor_FR_val->setValue( motors.FR );
		ui->motor_FR_val->setLeftLabel( motors.FR/8 );

		if( motors.isHex ) {
			ui->motor_CR_val->setValue( motors.CR );
			ui->motor_CR_val->setLeftLabel( motors.CR/8 );

			ui->motor_CL_val->setValue( motors.CL );
			ui->motor_CL_val->setRightLabel( motors.CL/8 );
		}

        ui->motor_BR_val->setValue( motors.BR );
		ui->motor_BR_val->setLeftLabel( motors.BR/8 );

        ui->motor_BL_val->setValue( motors.BL );
		ui->motor_BL_val->setRightLabel( motors.BL/8 );
    }

    if(bQuatChanged) {
        //if(tcTabs.SelectedTab == tpStatus)
        {
			ui->Orientation_display->setQuat(q);
			ui->Orientation_Accel->setQuat(q);

			QMatrix3x3 m;
			m = QuatToMatrix( q );

			float roll =   asin( m(1,0) ) * (180.0f/PI);
			float pitch =  asin( m(1,2) ) * (180.0f/PI);
			float yaw =  -atan2( m(2,0), m(2,2) ) * (180.0f/PI);

			ui->lblRoll->setText( QString::number( roll, 'f', 1) );
			ui->lblPitch->setText( QString::number( pitch, 'f', 1) );
			ui->lblYaw->setText( QString::number( yaw, 'f', 1) );


			// Use the matrix and magnetometer here to experiment

			ui->Horizon_display->setAngles( roll, pitch );
			ui->Heading_display->setHeading(yaw);
        }
	}

	if( bSensorsChanged )
	{
		if( ui->tabWidget->currentWidget() == ui->tpGyroCalib )
		{
			static int calibSampleIndex = 0;

			LFSample sample;
			sample.t = sensors.Temp;
			sample.x = sensors.GyroX;
			sample.y = sensors.GyroY;
			sample.z = sensors.GyroZ;

			ui->gCalibTemp->setValue( sensors.Temp );
			ui->gCalibX->setValue( sensors.GyroX );
			ui->gCalibY->setValue( sensors.GyroY );
			ui->gCalibZ->setValue( sensors.GyroZ );

			calibSampleIndex++;

			bool bDoRedraw = (calibSampleIndex & 3) == 3;
			ui->lfGyroGraph->AddSample(sample , bDoRedraw );

			if( (calibSampleIndex & 15) == 0 ) {
				QString str = QString::asprintf("Range : %d  (60 or more is ideal)", (int)ui->lfGyroGraph->range );
				ui->lblGyroSampleRange->setText( str );

				str = QString::asprintf("Noise : %0.1f  (15 or less is ideal)", ui->lfGyroGraph->noise );
				ui->lblGyroSampleQuality->setText( str );

				if( ui->lfGyroGraph->range >= 60.0 )		str = "QLabel { background: #60ff60 }";	// green
				else if( ui->lfGyroGraph->range >= 40.0 )	str = "QLabel { background: #ffff00 }";	// yellow
				else										str = "QLabel { background: #ff8080 }";	// red

				ui->lblGyroSampleRange->setStyleSheet( str );

				if( ui->lfGyroGraph->noise <= 15.0 )		str = "QLabel { background: #60ff60 }";	// green
				else if( ui->lfGyroGraph->noise <= 25.0 )	str = "QLabel { background: #ffff00 }";	// yellow
				else										str = "QLabel { background: #ff8080 }";	// red

				ui->lblGyroSampleQuality->setStyleSheet( str );
			}

			if( bDoRedraw == false )
			{
				int scaleX = 0, scaleY = 0, scaleZ = 0;

				if(fabs(ui->lfGyroGraph->dSlope.x ) > 0.00001)
					scaleX = (int)round( 1.0 / ui->lfGyroGraph->dSlope.x );
				if( scaleX >= 1024 ) scaleX = 0;

				if(fabs( ui->lfGyroGraph->dSlope.y ) > 0.00001)
					scaleY = (int)round( 1.0 / ui->lfGyroGraph->dSlope.y );
				if( scaleY >= 1024 ) scaleY = 0;

				if(fabs( ui->lfGyroGraph->dSlope.z ) > 0.00001)
					scaleZ = (int)round( 1.0 / ui->lfGyroGraph->dSlope.z );
				if( scaleZ >= 1024 ) scaleZ = 0;

				int offsetX = (int)round( ui->lfGyroGraph->dIntercept.x );
				int offsetY = (int)round( ui->lfGyroGraph->dIntercept.y );
				int offsetZ = (int)round( ui->lfGyroGraph->dIntercept.z );

				QString str;
				str = QString::number( scaleX );
				ui->lblGxScale->setText(str);

				str = QString::number( scaleY );
				ui->lblGyScale->setText(str);

				str = QString::number( scaleZ );
				ui->lblGzScale->setText(str);

				str = QString::number( offsetX );
				ui->lblGxOffset->setText(str);

				str = QString::number( offsetY );
				ui->lblGyOffset->setText(str);

				str = QString::number( offsetZ );
				ui->lblGzOffset->setText(str);
			}
		}
		else if( ui->tabWidget->currentWidget() == ui->tpAccelCalib )
		{
			ui->gAccelXCal->setValue( sensors.AccelX );
			ui->gAccelYCal->setValue( sensors.AccelY );
			ui->gAccelZCal->setValue( sensors.AccelZ );
		}
		else if( ui->tabWidget->currentWidget() == ui->tpSensors )
		{
			ui->lblGyroX->setText( QString::number(sensors.GyroX) );
			ui->lblGyroY->setText( QString::number(sensors.GyroY) );
			ui->lblGyroZ->setText( QString::number(sensors.GyroZ) );
			ui->lblGyroTemp->setText( QString::number(sensors.Temp) );

			ui->lblAccelX->setText( QString::number(sensors.AccelX) );
			ui->lblAccelY->setText( QString::number(sensors.AccelY) );
			ui->lblAccelZ->setText( QString::number(sensors.AccelZ) );

			ui->lblMagX->setText( QString::number(sensors.MagX) );
			ui->lblMagY->setText( QString::number(sensors.MagY) );
			ui->lblMagZ->setText( QString::number(sensors.MagZ) );

			sg->replot();
		}
	}

	if(bTargetQuatChanged)
	{
		ui->Orientation_display->setQuat2(cq);
	}

    if( bDebugChanged )
    {
		int verHigh = debugData.Version >> 8;
		int verMid  = (debugData.Version >> 4) & 15;
		int verLow  = (debugData.Version >> 0) & 15;

		labelFWVersion->setText( QString( "Firmware Version %1.%2.%3" ).arg(verHigh).arg(verMid).arg(verLow) );

		QString str = QString("CPU time (uS): %1 (min), %2 (max), %3 (avg)" )
				.arg( debugData.MinCycles * 64/80 ).arg( debugData.MaxCycles * 64/80 ).arg( debugData.AvgCycles * 64/80 );

		ui->lblCycles->setText( str );
    }

    if( bComputedChanged ) {
        ui->Altimeter_display->setAltitude( computed.AltiEst / 1000.0f );

        ui->rollPowerVal->setValue( computed.Roll );
        ui->rollPowerVal->setRightLabel( computed.Roll );

        ui->pitchPowerVal->setValue( computed.Pitch );
        ui->pitchPowerVal->setRightLabel( computed.Pitch );

        ui->yawPowerVal->setValue( computed.Yaw );
        ui->yawPowerVal->setRightLabel( computed.Yaw );

		if( ui->tabWidget->currentWidget() == ui->tpSensors )
		{
			ui->lblAltPressure->setText( QString::number(computed.Alt / 1000.0f, 'f', 2) );
			ui->lblAltiEst->setText( QString::number(computed.AltiEst / 1000.0f, 'f', 2) );
			ui->lblGroundHeight->setText( QString::number(computed.GroundHeight) );
		}
	}

	if( bPrefsChanged ) {
        ConfigureUIFromPreferences();
    }
}

void MainWindow::SendTestMotor(int index)
{
	if(comm.Connected()) {
		CancelThrottleCalibration();	// just in case
        index++;
		SendCommand( QString("M%1t%2").arg( index ).arg( index ) );	// Becomes M1t1, M2t2, etc..
    }
}

void MainWindow::SendTestBeeper(void)
{
	if(comm.Connected()) {
		CancelThrottleCalibration();	// just in case
		SendCommand( QString("MBZZ") );
	}
}

void MainWindow::SendTestLED(void)
{
	if(comm.Connected()) {
		CancelThrottleCalibration();	// just in case
		SendCommand( QString("MLED") );
	}
}

void MainWindow::SendThrottleCalibrate(void)
{
	if(comm.Connected()) {
		CancelThrottleCalibration();	// just in case
		SendCommand( QString("TCAL") );
	}
}



void MainWindow::CancelThrottleCalibration(void)
{
	if(ThrottleCalibrationCycle == 0) return;

	quint8 txBuffer[1] = {0};	// Escape from throttle setting
	comm.Send( txBuffer, 1 );
	QString str;
	ui->lblCalibrateDocs->setText(str);
	//ui->lblCalibrateDocs->setVisible(false);
	//ui->gbControlSetup->setVisible(true);
	ui->calibrateStack->setCurrentIndex(0);
	ThrottleCalibrationCycle = 0;
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
	(void)index;	// prevent compilation warning from unused variable

	Mode newMode;
	CancelThrottleCalibration();	// just in case

	if(ui->tabWidget->currentWidget() == ui->tpGyroCalib) {
		newMode = GyroCalibration;
	}
	if(ui->tabWidget->currentWidget() == ui->tpAccelCalib) {
		newMode = AccelCalibration;
	}
	else {
		newMode = SensorTest;
	}

	if(newMode == currentMode) {
		return;
	}

	// If we're switching OUT of one of the calibration modes
	// tell the flight controller to revert back to its drift-compensated settings
	if(currentMode == GyroCalibration) {
		SendCommand( "RGyr" );	// revert previous gyro calibration values
	}
	else if(currentMode == AccelCalibration) {
		SendCommand( "RAcl");	// revert previous accel calibration values
	}

	if(newMode == GyroCalibration) {
		SendCommand( "ZrGr" );	// zero gyro calibration settings
	}
	else if(newMode == AccelCalibration) {
		SendCommand( "ZeAc" );	// zero accelerometer calibration settings
	}

	currentMode = newMode;
}


void MainWindow::on_btnSafetyCheck_clicked()
{
	if( ui->btnSafetyCheck->isChecked() == false ) {
		CancelThrottleCalibration();	// If they press it DURING throttle calibration, it's an escape button
	}
}


void MainWindow::on_btnThrottleCalibrate_clicked()
{
QString str;
quint8 txBuffer[1];

	switch(ThrottleCalibrationCycle)
	{
	case 0:
		if( ui->btnSafetyCheck->isChecked() == false )
		{
			str = "You must verify that you have removed your propellers by pressing the button to the right.";
			AbortThrottleCalibrationWithMessage( str , 3 );
			return;
		}
		SendThrottleCalibrate();
		str = "Throttle calibration has started.  Be sure your flight battery is UNPLUGGED, then press the Throttle Calibration button again.  (Click any other button to cancel)";
		ui->calibrateStack->setCurrentIndex(1);
		//ui->lblCalibrateDocs->setVisible(true);
		ui->lblCalibrateDocs->setText(str);
		ThrottleCalibrationCycle = 1;

		// TODO - Should probably disable all other buttons, and make an abort button visible
		break;

	case 1:
		if( radio.BatteryVolts > 0 )
		{
			str = "You must disconnect your flight battery to calibrate your ESC throttle range.  Failure to do so is a serious safety hazard.";
			AbortThrottleCalibrationWithMessage( str , 5 );
			return;
		}

		txBuffer[0] = (quint8)0xFF;
		comm.Send( txBuffer, 1 );
		str = "Plug in your flight battery and wait for the ESCs to stop beeping (about 5 seconds), then press the Throttle Calibration button again";
		ui->lblCalibrateDocs->setText(str);
		ThrottleCalibrationCycle = 2;
		break;

	case 2:
		txBuffer[0] = 0;		// Finish
		comm.Send( txBuffer, 1 );
        	str = "After the ESCs stop beeping (about 5 seconds), press the Throttle Calibration button again.";
		ui->lblCalibrateDocs->setText(str);
        	ThrottleCalibrationCycle = 3;
		break;

	case 3:
		txBuffer[0] = 0;		// Finish
		comm.Send( txBuffer, 1 );
		str = "To complete the Throttle Calibration, unplug both the flight battery and the USB cable.";
		ui->lblCalibrateDocs->setText(str);

		qApp->processEvents();	// force the label we just updated to repaint

		QThread::sleep( 5 );
		str = "";
		ui->lblCalibrateDocs->setText(str);
		ThrottleCalibrationCycle = 0;
		
	        // TODO: Test for battery/usb, don't clear message until it is
	        // TODO: Re-enable all other buttons, hide the abort button
	        break;
	}
}

void MainWindow::AbortThrottleCalibrationWithMessage( QString & msg , int delay )
{
quint8 txBuffer[1];

	txBuffer[0] = (quint8)0x0;
	comm.Send( txBuffer, 1 );

	QString backup = ui->lblCalibrateDocs->styleSheet();
	ui->calibrateStack->setCurrentIndex(1);
	//ui->lblCalibrateDocs->setVisible(true);
	ui->lblCalibrateDocs->setText(msg);
	ui->lblCalibrateDocs->setStyleSheet("QLabel { background-color : orange; color : black; }");

	qApp->processEvents();	// force the label we just updated to repaint

	QThread::sleep( delay );
	ui->lblCalibrateDocs->setStyleSheet( backup );
	ui->lblCalibrateDocs->setText("");
	CancelThrottleCalibration();
}


void MainWindow::AttemptSetValue( QSlider * slider , int value )
{
	if( value < slider->minimum() || value > slider->maximum() ) return;
	slider->setValue( value );
}

void MainWindow::AttemptSetValue( QSpinBox * spin , int value )
{
	if( value < spin->minimum() || value > spin->maximum() ) return;
	spin->setValue( value );
}

void MainWindow::AttemptSetValue( QDoubleSpinBox * spin, double value )
{
	if( value < spin->minimum() || value > spin->maximum() ) return;
	spin->setValue( value );
}

void MainWindow::AttemptSetValue( QScrollBar* scroll, int value )
{
	if( value < scroll->minimum() || value > scroll->maximum() ) return;
	scroll->setValue( value );
}


void MainWindow::ConfigureUIFromPreferences(void)
{
	InternalChange = true;

	// Radio Setup
	//----------------------------------------------------------------------------
	ui->cbReceiverType->setCurrentIndex( prefs.ReceiverType );

	ui->revR_Channel1->setChecked( prefs.ThroScale < 0 );
	ui->revR_Channel2->setChecked( prefs.AileScale < 0 );
	ui->revR_Channel3->setChecked( prefs.ElevScale < 0 );
	ui->revR_Channel4->setChecked( prefs.RuddScale < 0 );
	ui->revR_Channel5->setChecked( prefs.GearScale < 0 );
	ui->revR_Channel7->setChecked( prefs.Aux1Scale < 0 );
	ui->revR_Channel6->setChecked( prefs.Aux2Scale < 0 );
	ui->revR_Channel8->setChecked( prefs.Aux3Scale < 0 );

	if( RadioMode == 2 ) {
		ui->cbR_Channel1->setCurrentIndex( prefs.ThroChannel );
		ui->cbR_Channel3->setCurrentIndex( prefs.ElevChannel );
	}
	else {
		ui->cbR_Channel1->setCurrentIndex( prefs.ElevChannel );
		ui->cbR_Channel3->setCurrentIndex( prefs.ThroChannel );
	}
	ui->cbR_Channel2->setCurrentIndex( prefs.AileChannel );
	ui->cbR_Channel4->setCurrentIndex( prefs.RuddChannel );
	ui->cbR_Channel5->setCurrentIndex( prefs.GearChannel );
	ui->cbR_Channel6->setCurrentIndex( prefs.Aux1Channel );
	ui->cbR_Channel7->setCurrentIndex( prefs.Aux2Channel );
	ui->cbR_Channel8->setCurrentIndex( prefs.Aux3Channel );


	float Source = prefs.AutoLevelRollPitch;
	Source = (Source * 2.0f) / (PI / 180.0f) * 1024.0f;
	AttemptSetValue( ui->hsAutoRollPitchSpeed, (int)(Source + 0.5f) );

	Source = prefs.AutoLevelYawRate;
	Source = (Source * 2.0f) / (PI / 180.0f) * 1024.0f * 250.0f;
	AttemptSetValue( ui->hsAutoYawSpeed, (int)(Source + 0.5f) / 10 );

	Source = prefs.ManualRollPitchRate;
	Source = (Source * 2.0f) / (PI / 180.0f) * 1024.0f * 250.0f;
	AttemptSetValue( ui->hsManualRollPitchSpeed, (int)(Source + 0.5f) / 10 );

	Source = prefs.ManualYawRate;
	Source = (Source * 2.0f) / (PI / 180.0f) * 1024.0f * 250.0f;
	AttemptSetValue( ui->hsManualYawSpeed, (int)(Source + 0.5f) / 10 );

	ui->cb_FlightMode_Up->setCurrentIndex( prefs.FlightMode[2] );
	ui->cb_FlightMode_Middle->setCurrentIndex( prefs.FlightMode[1] );
	ui->cb_FlightMode_Down->setCurrentIndex( prefs.FlightMode[0] );


	AttemptSetValue( ui->hsPitchRollExpo, prefs.AileExpo );
	AttemptSetValue( ui->hsYawExpo, prefs.RuddExpo );


	// Flight Control Setup
	//----------------------------------------------------------------------------

	ui->cbPitchRollLocked->setChecked( prefs.PitchRollLocked != 0 );
	AttemptSetValue( ui->hsPitchGain, prefs.PitchGain + 1 );
	AttemptSetValue( ui->hsRollGain, prefs.RollGain + 1 );
	AttemptSetValue( ui->hsYawGain, prefs.YawGain + 1 );
	AttemptSetValue( ui->hsAscentGain, prefs.AscentGain + 1 );
	AttemptSetValue( ui->hsAltiGain, prefs.AltiGain + 1 );
	//cbEnableAdvanced.Checked = (prefs.UseAdvancedPID != 0);

	ui->hsAccelCorrectionFilter->setValue( 256 - prefs.AccelCorrectionFilter );	// Higher number is actually less filtering
	ui->hsAccelCorrection->setValue( prefs.AccelCorrectionStrength );
	ui->hsThrustCorrection->setValue( prefs.ThrustCorrectionScale );


	// System Setup
	//----------------------------------------------------------------------------

	ui->cbUseBatteryMonitor->setChecked(prefs.UseBattMon == 1);

	AttemptSetValue( ui->sbLowThrottle, prefs.MinThrottle / 8 );
	AttemptSetValue( ui->sbArmedLowThrottle, prefs.MinThrottleArmed / 8 );
	AttemptSetValue( ui->sbHighThrottle, prefs.MaxThrottle / 8 );
	AttemptSetValue( ui->sbTestThrottle, prefs.ThrottleTest / 8 );
	ui->btnDisableMotors->setChecked(prefs.DisableMotors == 1);


	AttemptSetValue( ui->sbLowVoltageAlarmThreshold, (double)prefs.LowVoltageAlarmThreshold / 100.0 );
	AttemptSetValue( ui->sbVoltageOffset, (double)prefs.VoltageOffset / 100.0 );

	ui->cbLowVoltageAlarm->setChecked(prefs.LowVoltageAlarm != 0);

	switch( prefs.ArmDelay )
	{
		default:
		case 250: ui->cbArmingDelay->setCurrentIndex(0); break;	// 1 sec
		case 125: ui->cbArmingDelay->setCurrentIndex(1); break;	// 1/2 sec
		case 62:  ui->cbArmingDelay->setCurrentIndex(2); break;	// 1/4 sec
		case 0:   ui->cbArmingDelay->setCurrentIndex(3); break;	// none
	}

	switch(prefs.DisarmDelay)
	{
		default:
		case 250: ui->cbDisarmDelay->setCurrentIndex(0); break;	// 1 sec
		case 125: ui->cbDisarmDelay->setCurrentIndex(1); break;	// 1/2 sec
		case 62:  ui->cbDisarmDelay->setCurrentIndex(2); break;	// 1/4 sec
		case 0:   ui->cbDisarmDelay->setCurrentIndex(3); break;	// none
	}



	// Gyro Calibration
	//----------------------------------------------------------------------------


	// Accel Calibration
	//----------------------------------------------------------------------------

	if( prefs.PitchCorrectSin < -PI * 0.5 || prefs.PitchCorrectSin > PI * 0.5 ||
		prefs.PitchCorrectCos < -PI * 0.5 || prefs.PitchCorrectCos > PI * 0.5 )
	{
		prefs.PitchCorrectSin = 0.0f;
		prefs.PitchCorrectCos = 1.0f;
	}

	if( prefs.RollCorrectSin < -PI * 0.5 || prefs.RollCorrectSin > PI * 0.5 ||
		prefs.RollCorrectCos < -PI * 0.5 || prefs.RollCorrectCos > PI * 0.5)
	{
		prefs.RollCorrectSin = 0.0f;
		prefs.RollCorrectCos = 1.0f;
	}


	double RollAngle = asin( prefs.RollCorrectSin );
	double PitchAngle = asin( prefs.PitchCorrectSin );

	QString str = QString( "%1, %2, %3" ).arg( prefs.AccelOffsetX ).arg( prefs.AccelOffsetY ).arg( prefs.AccelOffsetZ );
	ui->lblAccelCalFinal->setText( str );

	AttemptSetValue( ui->udRollCorrection,  RollAngle * 180.0 / PI );
	AttemptSetValue( ui->udPitchCorrection, PitchAngle * 180.0 / PI );


	InternalChange = false;
}


// -----------------------------------------------
// System Test code
// -----------------------------------------------

void MainWindow::on_btnMotorTest_FL_pressed() {
	SendTestMotor(0);
}

void MainWindow::on_btnMotorTest_FR_pressed() {
	SendTestMotor(1);
}

void MainWindow::on_btnMotorTest_BR_pressed() {
	SendTestMotor(2);
}

void MainWindow::on_btnMotorTest_BL_pressed() {
	SendTestMotor(3);
}

void MainWindow::on_btnMotorTest_CR_clicked() {
	SendTestMotor(5);
}

void MainWindow::on_btnMotorTest_CL_clicked() {
	SendTestMotor(6);
}

void MainWindow::on_btnBeeper_pressed() {
	SendTestBeeper();
}

void MainWindow::on_btnLED_pressed() {
	SendTestLED();
}

void MainWindow::UpdateElev8Preferences(void)
{
	// Send prefs
	prefs.Checksum = Prefs_CalculateChecksum( prefs );
	SendCommand( "UPrf" );	// Update preferences

	QThread::msleep( 10 );	// Sleep a moment to let the Elev8-FC get ready

	quint8 * prefBytes = (quint8 *)&prefs;

	// Have to slow this down a little during transmission because the buffer
	// on the other end is small to save ram, and we don't have flow control
	for(int i = 0; i < (int)sizeof(prefs); i+=4)
	{
		comm.Send( prefBytes + i, 4 );	// send 4 bytes at a time (assumes the struct is a multiple of 4 bytes)
		QThread::msleep( 5 );			// sleep for a moment to let the Prop commit them
	}

	// Query prefs (forces to be applied to UI)
	SendCommand( "QPRF" );
}


// -----------------------------------------------
// Radio Setup code
// -----------------------------------------------

void MainWindow::SetReverseChannel( int channel , bool bReverse )
{
	if( channel < 1 || channel > 8 ) return;

	short scale = abs(prefs.ChannelScale( channel-1 ));
	if( bReverse ) scale *= -1;

	// Don't re-write the prefs if we set it to the same value
	if( prefs.ChannelScale( channel-1) == scale ) return;

	prefs.ChannelScale( channel-1 ) = scale;
	UpdateElev8Preferences();
}


void MainWindow::on_revR_Channel1_clicked(bool checked) {
	SetReverseChannel( 1 , checked );
}

void MainWindow::on_revR_Channel2_clicked(bool checked) {
	SetReverseChannel( 2 , checked );
}

void MainWindow::on_revR_Channel3_clicked(bool checked) {
	SetReverseChannel( 3 , checked );
}

void MainWindow::on_revR_Channel4_clicked(bool checked) {
	SetReverseChannel( 4 , checked );
}

void MainWindow::on_revR_Channel5_clicked(bool checked) {
	SetReverseChannel( 5 , checked );
}

void MainWindow::on_revR_Channel6_clicked(bool checked) {
	SetReverseChannel( 6 , checked );
}

void MainWindow::on_revR_Channel7_clicked(bool checked) {
	SetReverseChannel( 7 , checked );
}

void MainWindow::on_revR_Channel8_clicked(bool checked) {
	SetReverseChannel( 8 , checked );
}


void MainWindow::on_btnUploadRadioChanges_clicked()
{
	prefs.ReceiverType = ui->cbReceiverType->currentIndex();

	int Value = ui->hsAutoRollPitchSpeed->value();
	float Rate = ((float)Value / 1024.0f) * (PI / 180.0f) * 0.5f;
	prefs.AutoLevelRollPitch = Rate;

	Value = ui->hsAutoYawSpeed->value() * 10;
	Rate = (((float)Value / 250.0f) / 1024.0f) * (PI / 180.0f) * 0.5f;
	prefs.AutoLevelYawRate = Rate;

	Value = ui->hsManualRollPitchSpeed->value() * 10;
	Rate = (((float)Value / 250.0f) / 1024.0f) * (PI / 180.0f) * 0.5f;
	prefs.ManualRollPitchRate = Rate;

	Value = ui->hsManualYawSpeed->value() * 10;
	Rate = (((float)Value / 250.0f) / 1024.0f) * (PI / 180.0f) * 0.5f;
	prefs.ManualYawRate = Rate;

	prefs.FlightMode[0] = ui->cb_FlightMode_Down->currentIndex();
	prefs.FlightMode[1] = ui->cb_FlightMode_Middle->currentIndex();
	prefs.FlightMode[2] = ui->cb_FlightMode_Up->currentIndex();

	prefs.AileExpo = ui->hsPitchRollExpo->value();
	prefs.ElevExpo = ui->hsPitchRollExpo->value();
	prefs.RuddExpo = ui->hsYawExpo->value();

	UpdateElev8Preferences();
}

void MainWindow::on_btnReceiverReset_clicked()
{
	for(int i = 0; i < 8; i++)
	{
		prefs.ChannelIndex(i)  = i;
		prefs.ChannelScale(i)  = 1024;
		prefs.ChannelCenter(i) = 0;
	}
	UpdateElev8Preferences();
}

void MainWindow::on_btnReceiverCalibrate_clicked()
{
	QMessageBox msg;
	msg.setWindowTitle("Radio Calibration");
	msg.setText("Radio Calibration");
	msg.setInformativeText(
		"This calibration will measure the range of your radio transmitter inputs.\n"\
		"Move all the controls on your radio, including sticks, switches, and knobs\n"\
		"to their full extents, multiple times if possible.\n"\
		"\n"\
		"When finished, check the on-screen sticks against your radio and reverse\n"\
		"any controls that are responding incorrectly."
		);
	msg.setStandardButtons(QMessageBox::Ok);
	msg.exec();

	CalibrateControlsStep = 1;
	CalibrateTimer = 600;
}


void MainWindow::CheckCalibrateControls(void)
{
	switch(CalibrateControlsStep)
	{
		case 0:	return;	// Inactive

		case 1:
			for(int i = 0; i < 8; i++) {
				channelData[i] = ChannelData();	// reset the channel data struct
			}
			SendCommand("Rrad");	// reset radio scale & offsets
			CalibrateControlsStep++;
			return;

		case 2:
		{
			ui->calibrateStack->setCurrentIndex(1);

			QString str = QString( "Move all controls to their full extents multiple times to determine range\n"\
								   "When finished, you may need to reverse some controls\n"\
								   "%1").arg( CalibrateTimer );

			ui->lblRadioCalibrateDocs->setText( str );

			// wait a few cycles so we're not getting scaled packets from the FC from before we reset the scaling
			if( CalibrateTimer == 550 )
			{
				// Grab the initial position of all the controls.  If the user misses one, better to leave it than zero the range
				for( int i=0; i<8; i++ ) {
					channelStart[i] = radio[i];
					channelMoved[i] = false;
				}
			}
			else if( CalibrateTimer < 550 )
			{
				for(int i = 0; i < 8; i++)
				{
					if( abs(radio[i] - channelStart[i]) > 30 ) {
						channelMoved[i] = true;
					}
					if( radio[i] > channelData[i].max ) { channelData[i].max = radio[i]; }
					if( radio[i] < channelData[i].min ) { channelData[i].min = radio[i]; }
				}
			}

			if( --CalibrateTimer == 0 ) {
				CalibrateControlsStep++;
			}
			break;
		}

		case 3:
		{
			CalibrateControlsStep = 0;
			QString str;
			ui->calibrateStack->setCurrentIndex(0);
			ui->lblRadioCalibrateDocs->setText(str);

			// figure out scales
			for(int i = 0; i < 8; i++)
			{
				if( channelMoved[i] == false ) continue;	// Skip channels that didn't move

				int range = channelData[i].max - channelData[i].min;
				channelData[i].scale = (int)((1.0f / ((float)range / 2048.0f)) * 1024.0f);
				if( prefs.ChannelScale(i) < 0 ) {
					channelData[i].scale *= -1;
				}
				channelData[i].center = (channelData[i].min + channelData[i].max) / 2;
			}

			// apply to prefs
			if( channelMoved[0] ) {
				prefs.ThroScale = (short)channelData[0].scale;
				prefs.ThroCenter = (short)channelData[0].center;
			}
			if( channelMoved[1] ) {
				prefs.AileScale = (short)channelData[1].scale;
				prefs.AileCenter = (short)channelData[1].center;
			}

			if( channelMoved[2] ) {
				prefs.ElevScale = (short)channelData[2].scale;
				prefs.ElevCenter = (short)channelData[2].center;
			}

			if( channelMoved[3] ) {
				prefs.RuddScale = (short)channelData[3].scale;
				prefs.RuddCenter = (short)channelData[3].center;
			}

			if( channelMoved[4] ) {
				prefs.GearScale = (short)channelData[4].scale;
				prefs.GearCenter = (short)channelData[4].center;
			}

			if( channelMoved[5] ) {
				prefs.Aux1Scale = (short)channelData[5].scale;
				prefs.Aux1Center = (short)channelData[5].center;
			}

			if( channelMoved[6] ) {
				prefs.Aux2Scale = (short)channelData[6].scale;
				prefs.Aux2Center = (short)channelData[6].center;
			}

			if( channelMoved[7] ) {
				prefs.Aux3Scale = (short)channelData[7].scale;
				prefs.Aux3Center = (short)channelData[7].center;
			}

			UpdateElev8Preferences();
			break;
		}
	}
}

/*
void MainWindow::CheckCalibrateControls(void)
{
	switch(CalibrateControlsStep)
	{
		case 0:	return;	// Inactive

		case 1:
			for(int i = 0; i < 8; i++) {
				channelData[i] = ChannelData();	// reset the channel data struct
			}
			SendCommand("Rrad");	// reset radio scale & offsets
			CalibrateControlsStep++;
			return;

		case 2:
		{
			ui->calibrateStack->setCurrentIndex(1);

			QString str = QString( \
                    "Move both STICKS all the way to the RIGHT and UP, and all\n" \
					"SWITCHES/KNOBS to their MAXIMUM value position. (1 or 2/clockwise)\n" \
					"(Controls above may respond incorrectly) - %1").arg( CalibrateTimer );

			ui->lblRadioCalibrateDocs->setText( str );

			//if(CalibrateControlsStep == 200) {
			//	SystemSounds.Exclamation.Play();
			//}

			if(CalibrateTimer < 10)
			{
				for(int i = 0; i < 8; i++)
				{
					if( abs(channelData[i].max) < abs( radio[i] ) ) {
						channelData[i].max = radio[i];
					}
				}
			}

			if( --CalibrateTimer == 0 )
			{
				CalibrateControlsStep++;
				CalibrateTimer = 500;
			}
			break;
		}

		case 3:
		{
			QString str = QString( \
                    "Move both STICKS all the way to the LEFT and DOWN, and all\n" \
				    "SWITCHES/KNOBS to their MINIMUM value position (0/counter-clockwise) - %1").arg( CalibrateTimer );

			ui->lblRadioCalibrateDocs->setText( str );

			//if(CalibrateControlsStep == 200) {
			//	SystemSounds.Exclamation.Play();
			//}

			if(CalibrateTimer < 10)
			{
				for(int i = 0; i < 8; i++)
				{
					if( abs(channelData[i].min) < abs( radio[i] ) ) {
						channelData[i].min = radio[i];
					}
				}
			}

			if( --CalibrateTimer == 0 )
			{
				CalibrateControlsStep++;
				CalibrateTimer = 500;
			}
			break;
		}

		case 4:
		{
			CalibrateControlsStep = 0;
			QString str;
			ui->calibrateStack->setCurrentIndex(0);
			ui->lblRadioCalibrateDocs->setText(str);

			// figure out reverses
			for(int i = 0; i < 8; i++)
			{
				if(channelData[i].min > channelData[i].max) {
					channelData[i].reverse = true;	// Needs to be reversed with respect to what's already stored
				}

				int range = abs( channelData[i].min - channelData[i].max );
				channelData[i].scale = (int)((1.0f / ((float)range / 2048.0f)) * 1024.0f);
				channelData[i].center = (channelData[i].min + channelData[i].max) / 2;
				if(channelData[i].reverse) channelData[i].scale *= -1;
			}

			// apply to prefs
			prefs.ThroScale = (short)channelData[0].scale;
			prefs.AileScale = (short)channelData[1].scale;
			prefs.ElevScale = (short)channelData[2].scale;
			prefs.RuddScale = (short)channelData[3].scale;
			prefs.GearScale = (short)channelData[4].scale;
			prefs.Aux1Scale = (short)channelData[5].scale;
			prefs.Aux2Scale = (short)channelData[6].scale;
			prefs.Aux3Scale = (short)channelData[7].scale;


			prefs.ThroCenter = (short)channelData[0].center;
			prefs.AileCenter = (short)channelData[1].center;
			prefs.ElevCenter = (short)channelData[2].center;
			prefs.RuddCenter = (short)channelData[3].center;
			prefs.GearCenter = (short)channelData[4].center;
			prefs.Aux1Center = (short)channelData[5].center;
			prefs.Aux2Center = (short)channelData[6].center;
			prefs.Aux3Center = (short)channelData[7].center;

			UpdateElev8Preferences();
			break;
		}
	}
}
*/


void MainWindow::SetChannelMapping( int DestChannel, int SourceChannel )
{
	if( DestChannel < 0 || DestChannel > 7 || SourceChannel < 0 || SourceChannel > 7 ) return;

	prefs.ChannelIndex(DestChannel) = SourceChannel;
	UpdateElev8Preferences();
}

void MainWindow::on_cbR_Channel1_currentIndexChanged(int index) {
	if(!InternalChange) {
		if( RadioMode == 2 ) {
			SetChannelMapping(0, index);
		}
		else {
			SetChannelMapping(2, index);
		}
	}
}

void MainWindow::on_cbR_Channel2_currentIndexChanged(int index) {
	if(!InternalChange) SetChannelMapping(1, index);
}

void MainWindow::on_cbR_Channel3_currentIndexChanged(int index) {
	if(!InternalChange) {
		if( RadioMode == 2 ) {
			SetChannelMapping(2, index);
		}
		else {
			SetChannelMapping(0, index);
		}
	}
}

void MainWindow::on_cbR_Channel4_currentIndexChanged(int index) {
	if(!InternalChange) SetChannelMapping(3, index);
}

void MainWindow::on_cbR_Channel5_currentIndexChanged(int index) {
	if(!InternalChange) SetChannelMapping(4, index);
}

void MainWindow::on_cbR_Channel6_currentIndexChanged(int index) {
	if(!InternalChange) SetChannelMapping(5, index);
}

void MainWindow::on_cbR_Channel7_currentIndexChanged(int index) {
	if(!InternalChange) SetChannelMapping(6, index);
}

void MainWindow::on_cbR_Channel8_currentIndexChanged(int index) {
	if(!InternalChange) SetChannelMapping(7, index);
}



void MainWindow::on_hsAutoRollPitchSpeed_valueChanged(int value)
{
	QString str = QString("%1 deg").arg( value );
	ui->lblAutoRollPitchSpeed->setText( str );
}

void MainWindow::on_hsAutoYawSpeed_valueChanged(int value)
{
	QString str = QString("%1 deg/s").arg( value*10 );
	ui->lblAutoYawSpeed->setText( str );
}

void MainWindow::on_hsManualRollPitchSpeed_valueChanged(int value)
{
	QString str = QString("%1 deg/s").arg( value*10 );
	ui->lblManualRollPitchSpeed->setText( str );
}

void MainWindow::on_hsManualYawSpeed_valueChanged(int value)
{
	QString str = QString("%1 deg/s").arg( value*10 );
	ui->lblManualYawSpeed->setText( str );
}

void MainWindow::on_hsPitchRollExpo_valueChanged(int value)
{
	QString str = QString("%1 %").arg( value );
	ui->lblPitchRollExpo->setText( str );
}

void MainWindow::on_hsYawExpo_valueChanged(int value)
{
	QString str = QString("%1 %").arg( value );
	ui->lblYawExpo->setText( str );
}

void MainWindow::on_hsAccelCorrectionFilter_valueChanged(int value)
{
	QString str = QString::number((float)value / 256.f, 'f', 3);
	ui->lblAccelCorrectionFilter->setText( str );
}

void MainWindow::on_hsAccelCorrection_valueChanged(int value)
{
	QString str = QString::number((float)value / 256.f, 'f', 3);
	ui->lblAccelCorrection->setText( str );
}


void MainWindow::on_hsThrustCorrection_valueChanged(int value)
{
	QString str = QString::number((float) value / 256.f, 'f', 3);
	ui->lblThrustCorrection->setText( str );
}


// -----------------------------------------------
// Flight control setup code
// -----------------------------------------------

void MainWindow::on_hsPitchGain_valueChanged(int value)
{
	if(ui->cbPitchRollLocked->isChecked())
	{
		if(ui->hsRollGain->value() != value) {
			ui->hsRollGain->setValue( value );
		}
	}
	QString str = QString::number( value );
	ui->lblPitchGain->setText( str );
}

void MainWindow::on_hsRollGain_valueChanged(int value)
{
	if(ui->cbPitchRollLocked->isChecked())
	{
		if(ui->hsPitchGain->value() != value) {
			ui->hsPitchGain->setValue( value );
		}
	}
	QString str = QString::number( value );
	ui->lblRollGain->setText( str );
}

void MainWindow::on_cbPitchRollLocked_stateChanged(int arg1)
{
	(void)arg1;
	if(ui->cbPitchRollLocked->isChecked())
	{
		if( ui->hsRollGain->value() != ui->hsPitchGain->value() ) {
			ui->hsRollGain->setValue( ui->hsPitchGain->value() );
		}
	}
}

void MainWindow::on_hsYawGain_valueChanged(int value)
{
	QString str = QString::number( value );
	ui->lblYawGain->setText( str );
}


void MainWindow::on_hsAscentGain_valueChanged(int value)
{
	QString str = QString::number( value );
	ui->lblAscentGain->setText( str );
}

void MainWindow::on_hsAltiGain_valueChanged(int value)
{
	QString str = QString::number( value );
	ui->lblAltiGain->setText( str );
}

void MainWindow::on_btnUploadFlightChanges_clicked()
{
	prefs.PitchRollLocked = ui->cbPitchRollLocked->isChecked() ? (quint8)1 : (quint8)0;
	prefs.UseAdvancedPID = 0; // cbEnableAdvanced->isChecked() ? (quint8)1 : (quint8)0;

	prefs.PitchGain =  (quint8)(ui->hsPitchGain->value() - 1);
	prefs.RollGain =   (quint8)(ui->hsRollGain->value() - 1);
	prefs.YawGain =    (quint8)(ui->hsYawGain->value() - 1);
	prefs.AscentGain = (quint8)(ui->hsAscentGain->value() - 1);
	prefs.AltiGain =   (quint8)(ui->hsAltiGain->value() - 1);

	prefs.AccelCorrectionFilter = (short)(256 - ui->hsAccelCorrectionFilter->value());
	prefs.AccelCorrectionStrength = (unsigned char)ui->hsAccelCorrection->value();
	prefs.ThrustCorrectionScale = (short)ui->hsThrustCorrection->value();

	// Apply the prefs to the elev-8
	UpdateElev8Preferences();
}


// -----------------------------------------------
// System setup code
// -----------------------------------------------

void MainWindow::on_btnUploadSystemSetup_clicked()
{
	static qint16 DelayTable[] = {250, 125, 62, 0 };

	prefs.MinThrottle = (qint16)(ui->sbLowThrottle->value() * 8);
	prefs.MinThrottleArmed = (qint16)(ui->sbArmedLowThrottle->value() * 8);
	prefs.ThrottleTest = (qint16)(ui->sbTestThrottle->value() * 8);
	prefs.MaxThrottle = (qint16)(ui->sbHighThrottle->value() * 8);

	prefs.UseBattMon = (quint8)(ui->cbUseBatteryMonitor->isChecked() ? 1 : 0);
	prefs.LowVoltageAlarmThreshold = (qint16)(ui->sbLowVoltageAlarmThreshold->value() * 100);
	prefs.VoltageOffset = (qint16)(ui->sbVoltageOffset->value() * 100);
	prefs.LowVoltageAlarm = (quint8)(ui->cbLowVoltageAlarm->isChecked() ? 1 : 0);

	prefs.ArmDelay = DelayTable[ui->cbArmingDelay->currentIndex()];
	prefs.DisarmDelay = DelayTable[ui->cbDisarmDelay->currentIndex()];

	prefs.DisableMotors = (quint8)(ui->btnDisableMotors->isChecked() ? 1 : 0);

	UpdateElev8Preferences();
}



// -----------------------------------------------
// Gyro Calibrate code
// -----------------------------------------------

void MainWindow::on_btnRestartCalibration_clicked()
{
	ui->lfGyroGraph->Reset();
}

void MainWindow::on_btnUploadGyroCalibration_clicked()
{
	prefs.DriftScaleX =  (int)round( 1.0 / ui->lfGyroGraph->dSlope.x );
	prefs.DriftScaleY =  (int)round( 1.0 / ui->lfGyroGraph->dSlope.y );
	prefs.DriftScaleZ =  (int)round( 1.0 / ui->lfGyroGraph->dSlope.z );
	prefs.DriftOffsetX = (int)round( ui->lfGyroGraph->dIntercept.x );
	prefs.DriftOffsetY = (int)round( ui->lfGyroGraph->dIntercept.y );
	prefs.DriftOffsetZ = (int)round( ui->lfGyroGraph->dIntercept.z );

	UpdateElev8Preferences();
}


// -----------------------------------------------
// Accel Calibrate code
// -----------------------------------------------

void MainWindow::GetAccelAvgSasmple( int i )
{
	QLabel * labels[] = { ui->lblAccelCal1, ui->lblAccelCal2, ui->lblAccelCal3, ui->lblAccelCal4 };

	accXCal[i] = ui->gAccelXCal->getMovingAverage();
	accYCal[i] = ui->gAccelYCal->getMovingAverage();
	accZCal[i] = ui->gAccelZCal->getMovingAverage();

	QString str = QString("%1, %2, %3")
        .arg(QString::number(accXCal[i], 'f', 1))
        .arg(QString::number(accYCal[i], 'f', 1))
        .arg(QString::number(accZCal[i], 'f', 1));
       
	labels[i]->setText( str );
}

void MainWindow::on_btnAccelCal1_clicked() {
	GetAccelAvgSasmple(0);
}

void MainWindow::on_btnAccelCal2_clicked() {
	GetAccelAvgSasmple(1);
}

void MainWindow::on_btnAccelCal3_clicked() {
	GetAccelAvgSasmple(2);
}

void MainWindow::on_btnAccelCal4_clicked() {
	GetAccelAvgSasmple(3);
}

void MainWindow::on_btnAccelCalUpload_clicked()
{
	float fx = 0.0f, fy = 0.0f, fz = 0.0f;
	for(int i = 0; i < 4; i++)
	{
		fx += accXCal[i] * 0.25f;
		fy += accYCal[i] * 0.25f;
		fz += accZCal[i] * 0.25f;
	}

	int ax = (int)roundf( fx );
	int ay = (int)roundf( fy );
	int az = (int)roundf( fz );

	QString str = QString("%1, %2, %3").arg(ax).arg(ay).arg(az);
	ui->lblAccelCalFinal->setText( str );

	az -= 4096;	// OneG

	prefs.AccelOffsetX = ax;
	prefs.AccelOffsetY = ay;
	prefs.AccelOffsetZ = az;

	UpdateElev8Preferences();
}

void MainWindow::on_btnUploadAngleCorrection_clicked()
{
	// Upload calibration data
	double rollOffset = (float)((double)ui->udRollCorrection->value() * PI / 180.0);
	double pitchOffset = (float)((double)ui->udPitchCorrection->value() * PI / 180.0);

	prefs.RollCorrectSin =  sinf( rollOffset );
	prefs.RollCorrectCos =  cosf( rollOffset );
	prefs.PitchCorrectSin = sinf( pitchOffset );
	prefs.PitchCorrectCos = cosf( pitchOffset );

	UpdateElev8Preferences();
}

void MainWindow::on_actionAbout_triggered()
{
	QDialog * dlg = new AboutBox(this);
	dlg->show();
}


void MainWindow::on_btnClearData_clicked()
{
	for( unsigned int i=0; i<sizeof(graphs) / sizeof(graphs[0]); i++ ) {
		graphs[i]->removeDataAfter(-1.0);
	}
	SampleIndex = 0;
}


void MainWindow::on_btnToggleAll_clicked()
{
	// Get the state of the buttons
	int checkCount = 0;

	checkCount += ui->cbGyroTemp->checkState() == Qt::Checked;
	checkCount += ui->cbGyroX->checkState() == Qt::Checked;
	checkCount += ui->cbGyroY->checkState() == Qt::Checked;
	checkCount += ui->cbGyroZ->checkState() == Qt::Checked;
	checkCount += ui->cbAccelX->checkState() == Qt::Checked;
	checkCount += ui->cbAccelY->checkState() == Qt::Checked;
	checkCount += ui->cbAccelZ->checkState() == Qt::Checked;
	checkCount += ui->cbMagX->checkState() == Qt::Checked;
	checkCount += ui->cbMagY->checkState() == Qt::Checked;
	checkCount += ui->cbMagZ->checkState() == Qt::Checked;
	checkCount += ui->cbAlti->checkState() == Qt::Checked;
	checkCount += ui->cbAltiEst->checkState() == Qt::Checked;
	checkCount += ui->cbLaserHeight->checkState() == Qt::Checked;
	checkCount += ui->cbPitch->checkState() == Qt::Checked;
	checkCount += ui->cbRoll->checkState() == Qt::Checked;
	checkCount += ui->cbYaw->checkState() == Qt::Checked;
	checkCount += ui->cbVoltage->checkState() == Qt::Checked;

	// figure out which is the more prevalent state
	bool MostlyChecked = ( checkCount > (signed)(sizeof(graphs)/sizeof(graphs[0])) / 2 );
	bool newState = !MostlyChecked;

	// set all buttons to the inverse
	ui->cbGyroTemp->setChecked(newState);
	ui->cbGyroX->setChecked(newState);
	ui->cbGyroY->setChecked(newState);
	ui->cbGyroZ->setChecked(newState);
	ui->cbAccelX->setChecked(newState);
	ui->cbAccelY->setChecked(newState);
	ui->cbAccelZ->setChecked(newState);
	ui->cbMagX->setChecked(newState);
	ui->cbMagY->setChecked(newState);
	ui->cbMagZ->setChecked(newState);
	ui->cbAlti->setChecked(newState);
	ui->cbAltiEst->setChecked(newState);
	ui->cbLaserHeight->setChecked(newState);
	ui->cbPitch->setChecked(newState);
	ui->cbRoll->setChecked(newState);
	ui->cbYaw->setChecked(newState);
	ui->cbVoltage->setChecked(newState);

	for( unsigned int i=0; i<sizeof(graphs)/sizeof(graphs[0]); i++) {
		graphs[i]->setVisible(newState);
	}
}


void MainWindow::on_cbGyroX_clicked(bool checked)		{graphs[ 0]->setVisible( checked ); }
void MainWindow::on_cbGyroY_clicked(bool checked)		{graphs[ 1]->setVisible( checked ); }
void MainWindow::on_cbGyroZ_clicked(bool checked)		{graphs[ 2]->setVisible( checked ); }
void MainWindow::on_cbAccelX_clicked(bool checked)		{graphs[ 3]->setVisible( checked ); }
void MainWindow::on_cbAccelY_clicked(bool checked)		{graphs[ 4]->setVisible( checked ); }
void MainWindow::on_cbAccelZ_clicked(bool checked)		{graphs[ 5]->setVisible( checked ); }
void MainWindow::on_cbMagX_clicked(bool checked)		{graphs[ 6]->setVisible( checked ); }
void MainWindow::on_cbMagY_clicked(bool checked)		{graphs[ 7]->setVisible( checked ); }
void MainWindow::on_cbMagZ_clicked(bool checked)		{graphs[ 8]->setVisible( checked ); }
void MainWindow::on_cbGyroTemp_clicked(bool checked)	{graphs[ 9]->setVisible( checked ); }
void MainWindow::on_cbAlti_clicked(bool checked)		{graphs[10]->setVisible( checked ); }
void MainWindow::on_cbAltiEst_clicked(bool checked)		{graphs[11]->setVisible( checked ); }
void MainWindow::on_cbLaserHeight_clicked(bool checked) {graphs[12]->setVisible( checked ); }
void MainWindow::on_cbPitch_clicked(bool checked)		{graphs[13]->setVisible( checked ); }
void MainWindow::on_cbRoll_clicked(bool checked)		{graphs[14]->setVisible( checked ); }
void MainWindow::on_cbYaw_clicked(bool checked)			{graphs[15]->setVisible( checked ); }
void MainWindow::on_cbVoltage_clicked(bool checked)		{graphs[16]->setVisible( checked ); }


void MainWindow::on_actionExport_Settings_to_File_triggered()
{
	// Get the filename from the user
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save Settings File"), QDir::currentPath(), tr("Elev8 Settings Files (*.elev8set *.xml)"));
	if (fileName.isEmpty())
		return;

	QFile file(fileName);
	if(!file.open(QFile::WriteOnly | QFile::Text)) {
		return;
	}

	// Serialize the settings data into an XML file
	WriteSettings( &file );

	file.close();
}


void MainWindow::on_actionImport_Settings_from_File_triggered()
{
	// Get the filename from the user
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open Settings File"), QDir::currentPath(), tr("Elev8 Settings Files (*.elev8set *.xml)"));
	if (fileName.isEmpty())
		return;

	QFile file(fileName);
	if(!file.open(QFile::ReadOnly | QFile::Text)) {
		return;
	}

	// Deserialize the settings data from the XML file
	ReadSettings( &file );

	file.close();
}


// With QString::number handling overloads for most types, this function should work for just about anything
template<typename TYPE> static void WritePref( QXmlStreamWriter & writer , const char * pPrefName , TYPE Value )
{
	writer.writeStartElement( pPrefName );
	writer.writeAttribute( "Value" , QString::number(Value) );
	writer.writeEndElement();
}


void MainWindow::WriteSettings( QIODevice *file )
{
	QXmlStreamWriter writer( file );
	writer.setAutoFormatting(true);
	writer.writeStartDocument();

	writer.writeStartElement("Elev8-Prefs");

	QString verString = QString( "%1.%2" ).arg(debugData.Version >> 8).arg( debugData.Version & 255, 2, 10, QChar('0'));
	writer.writeAttribute("Version", verString );

	WritePref( writer, "DriftScaleX", prefs.DriftScaleX );
	WritePref( writer, "DriftScaleY", prefs.DriftScaleY );
	WritePref( writer, "DriftScaleZ", prefs.DriftScaleZ );

	WritePref( writer, "DriftOffsetX", prefs.DriftOffsetX );
	WritePref( writer, "DriftOffsetY", prefs.DriftOffsetY );
	WritePref( writer, "DriftOffsetZ", prefs.DriftOffsetZ );

	WritePref( writer, "AccelOffsetX", prefs.AccelOffsetX );
	WritePref( writer, "AccelOffsetY", prefs.AccelOffsetY );
	WritePref( writer, "AccelOffsetZ", prefs.AccelOffsetZ );

	WritePref( writer, "MagOfsX", prefs.MagOfsX );
	WritePref( writer, "MagOfsY", prefs.MagOfsY );
	WritePref( writer, "MagOfsZ", prefs.MagOfsZ );

	WritePref( writer, "MagScaleX", prefs.MagScaleX );
	WritePref( writer, "MagScaleY", prefs.MagScaleY );
	WritePref( writer, "MagScaleZ", prefs.MagScaleZ );

	WritePref( writer, "RollCorrectSin", prefs.RollCorrectSin );
	WritePref( writer, "RollCorrectCos", prefs.RollCorrectCos );

	WritePref( writer, "PitchCorrectSin", prefs.PitchCorrectSin );
	WritePref( writer, "PitchCorrectCos", prefs.PitchCorrectCos );


	float RateScale = 2.0f / (PI/180.0f) * 1024.0;
	WritePref( writer, "AutoLevelRollPitch", prefs.AutoLevelRollPitch * RateScale );
	WritePref( writer, "AutoLevelYawRate", prefs.AutoLevelYawRate * RateScale * 200.0f );
	WritePref( writer, "ManualRollPitchRate", prefs.ManualRollPitchRate * RateScale * 200.0f );
	WritePref( writer, "ManualYawRate", prefs.ManualYawRate * RateScale * 200.0f );

	WritePref( writer, "PitchGain", prefs.PitchGain );
	WritePref( writer, "RollGain", prefs.RollGain );
	WritePref( writer, "YawGain", prefs.YawGain );
	WritePref( writer, "AscentGain", prefs.AscentGain );

	WritePref( writer, "AltiGain", prefs.AltiGain );
	WritePref( writer, "PitchRollLocked", prefs.PitchRollLocked );
	WritePref( writer, "UseAdvancedPID", prefs.UseAdvancedPID );
	WritePref( writer, "ReceiverType", prefs.ReceiverType );

	WritePref( writer, "UseBattMon", prefs.UseBattMon );
	WritePref( writer, "DisableMotors", prefs.DisableMotors );
	WritePref( writer, "LowVoltageAlarm", prefs.LowVoltageAlarm );
	WritePref( writer, "LowVoltageAscentLimit", prefs.LowVoltageAscentLimit );

	WritePref( writer, "ThrottleTest", prefs.ThrottleTest );
	WritePref( writer, "MinThrottle", prefs.MinThrottle );
	WritePref( writer, "MaxThrottle", prefs.MaxThrottle );
	WritePref( writer, "CenterThrottle", prefs.CenterThrottle );
	WritePref( writer, "MinThrottleArmed", prefs.MinThrottleArmed );
	WritePref( writer, "ArmDelay", prefs.ArmDelay );
	WritePref( writer, "DisarmDelay", prefs.DisarmDelay );

	WritePref( writer, "ThrustCorrectionScale", prefs.ThrustCorrectionScale );
	WritePref( writer, "AccelCorrectionFilter", prefs.AccelCorrectionFilter );

	WritePref( writer, "VoltageOffset", prefs.VoltageOffset );
	WritePref( writer, "LowVoltageAlarmThreshold", prefs.LowVoltageAlarmThreshold );

	WritePref( writer, "FlightMode_0", prefs.FlightMode[0] );
	WritePref( writer, "FlightMode_1", prefs.FlightMode[1] );
	WritePref( writer, "FlightMode_2", prefs.FlightMode[2] );
	WritePref( writer, "AccelCorrectionStrength", prefs.AccelCorrectionStrength );

	WritePref( writer, "AileExpo", prefs.AileExpo );
	WritePref( writer, "ElevExpo", prefs.ElevExpo );
	WritePref( writer, "RuddExpo", prefs.RuddExpo );

	WritePref( writer, "ThroChannel", prefs.ThroChannel );
	WritePref( writer, "AileChannel", prefs.AileChannel );
	WritePref( writer, "ElevChannel", prefs.ElevChannel );
	WritePref( writer, "RuddChannel", prefs.RuddChannel );
	WritePref( writer, "GearChannel", prefs.GearChannel );
	WritePref( writer, "Aux1Channel", prefs.Aux1Channel );
	WritePref( writer, "Aux2Channel", prefs.Aux2Channel );
	WritePref( writer, "Aux3Channel", prefs.Aux3Channel );

	WritePref( writer, "ThroScale", prefs.ThroScale );
	WritePref( writer, "ThroCenter", prefs.ThroCenter );

	WritePref( writer, "AileScale", prefs.AileScale );
	WritePref( writer, "AileCenter", prefs.AileCenter );

	WritePref( writer, "ElevScale", prefs.ElevScale );
	WritePref( writer, "ElevCenter", prefs.ElevCenter );

	WritePref( writer, "RuddScale", prefs.RuddScale );
	WritePref( writer, "RuddCenter", prefs.RuddCenter );

	WritePref( writer, "GearScale", prefs.GearScale );
	WritePref( writer, "GearCenter", prefs.GearCenter );

	WritePref( writer, "Aux1Scale", prefs.Aux1Scale );
	WritePref( writer, "Aux1Center", prefs.Aux1Center );

	WritePref( writer, "Aux2Scale", prefs.Aux2Scale );
	WritePref( writer, "Aux2Center", prefs.Aux2Center );

	WritePref( writer, "Aux3Scale", prefs.Aux3Scale );
	WritePref( writer, "Aux3Center", prefs.Aux3Center );

	writer.writeEndElement();	// prefs block
	writer.writeEndDocument();
}

void MainWindow::ReadSettings( QIODevice *file )
{
	QXmlStreamReader reader;

	// Read the settings from the XML file
	reader.setDevice( file );
	reader.readNext();

	while(!reader.atEnd())
	{
		if(reader.isStartElement())
		{
			if(reader.name() == "Elev8-Prefs")
			{
				if( reader.attributes().length() > 0 )
				{
					QXmlStreamAttribute attr = reader.attributes()[0];
					if( attr.name() == "Version" )
					{
						QString val = attr.value().toString();
						ReadSettingsContents( reader );
					}
				}
			}
			else {
				// This is not an elev8 settings file
				// TODO: Display an error
				return;
			}
			reader.readNext();
		}
		else if( reader.isEndElement() ) {
			reader.readNext();
		}
		else
			reader.readNext();
	}

	// Finally, set the UI from the prefs, and upload to the controller
	ConfigureUIFromPreferences();
	UpdateElev8Preferences();
}


static bool ReadInt( QXmlStreamReader & reader , int & val )
{
	bool ok = false;
	QXmlStreamAttribute attr = reader.attributes()[0];
	if( attr.name() == "Value" ) {
		int temp = attr.value().toInt( &ok );
		if( ok ) val = temp;
	}
	return ok;
}

static bool ReadInt( QXmlStreamReader & reader , short & val )
{
	bool ok = false;
	QXmlStreamAttribute attr = reader.attributes()[0];
	if( attr.name() == "Value" ) {
		int temp = attr.value().toInt( &ok );
		if( ok ) val = (short)temp;
	}
	return ok;
}

static bool ReadInt( QXmlStreamReader & reader , byte & val )
{
	bool ok = false;
	QXmlStreamAttribute attr = reader.attributes()[0];
	if( attr.name() == "Value" ) {
		int temp = attr.value().toInt( &ok );
		if( ok ) val = (byte)temp;
	}
	return ok;
}


static bool ReadFloat( QXmlStreamReader & reader , float & val , float scale = 1.0f )
{
	bool ok = false;
	QXmlStreamAttribute attr = reader.attributes()[0];
	if( attr.name() == "Value" ) {
		float temp = attr.value().toFloat( &ok );
		if( ok ) val = temp / scale;
	}
	return ok;
}


void MainWindow::ReadSettingsContents( QXmlStreamReader & reader )
{
	reader.readNext();

	const float RateScale = 2.0f / (PI/180.0f) * 1024.0;

	while( !(reader.isEndElement() && reader.name() == "Elev8-Prefs") )
	{
		if( reader.isStartElement() )
		{
			if(      reader.name() == "DriftScaleX")			ReadInt(reader, prefs.DriftScaleX);
			else if( reader.name() == "DriftScaleY" )			ReadInt(reader, prefs.DriftScaleY);
			else if( reader.name() == "DriftScaleZ")			ReadInt(reader, prefs.DriftScaleZ);
			else if( reader.name() == "DriftOffsetX")			ReadInt(reader, prefs.DriftOffsetX);
			else if( reader.name() == "DriftOffsetY")			ReadInt(reader, prefs.DriftOffsetY);
			else if( reader.name() == "DriftOffsetZ")			ReadInt(reader, prefs.DriftOffsetZ);

			else if( reader.name() == "AccelOffsetX")			ReadInt(reader, prefs.AccelOffsetX);
			else if( reader.name() == "AccelOffsetY")			ReadInt(reader, prefs.AccelOffsetY);
			else if( reader.name() == "AccelOffsetZ")			ReadInt(reader, prefs.AccelOffsetZ);

			else if( reader.name() == "MagOfsX")				ReadInt(reader, prefs.MagOfsX);
			else if( reader.name() == "MagOfsY")				ReadInt(reader, prefs.MagOfsY);
			else if( reader.name() == "MagOfsZ")				ReadInt(reader, prefs.MagOfsZ);
			else if( reader.name() == "MagScaleX")				ReadInt(reader, prefs.MagScaleX);
			else if( reader.name() == "MagScaleY")				ReadInt(reader, prefs.MagScaleY);
			else if( reader.name() == "MagScaleZ")				ReadInt(reader, prefs.MagScaleZ);

			else if( reader.name() == "RollCorrectSin")			ReadFloat(reader, prefs.RollCorrectSin);
			else if( reader.name() == "RollCorrectCos")			ReadFloat(reader, prefs.RollCorrectCos);
			else if( reader.name() == "PitchCorrectSin")		ReadFloat(reader, prefs.PitchCorrectSin);
			else if( reader.name() == "PitchCorrectCos")		ReadFloat(reader, prefs.PitchCorrectCos);

			else if( reader.name() == "AutoLevelRollPitch")		{
				ReadFloat(reader, prefs.AutoLevelRollPitch, RateScale );
			}
			else if( reader.name() == "AutoLevelYawRate")		{
				ReadFloat(reader, prefs.AutoLevelYawRate, RateScale * 200.0f );
			}
			else if( reader.name() == "ManualRollPitchRate")	{
				ReadFloat(reader, prefs.ManualRollPitchRate, RateScale * 200.0f );
			}
			else if( reader.name() == "ManualYawRate")			{
				ReadFloat(reader, prefs.ManualYawRate, RateScale * 200.0f );
			}

			else if( reader.name() == "PitchGain")				ReadInt(reader, prefs.PitchGain);
			else if( reader.name() == "RollGain")				ReadInt(reader, prefs.RollGain);
			else if( reader.name() == "YawGain")				ReadInt(reader, prefs.YawGain);
			else if( reader.name() == "AscentGain")				ReadInt(reader, prefs.AscentGain);
			else if( reader.name() == "AltiGain")				ReadInt(reader, prefs.AltiGain);
			else if( reader.name() == "PitchRollLocked")		ReadInt(reader, prefs.PitchRollLocked);
			else if( reader.name() == "UseAdvancedPID")			ReadInt(reader, prefs.UseAdvancedPID);

			else if( reader.name() == "ReceiverType")			ReadInt(reader, prefs.ReceiverType);
			else if( reader.name() == "UseBattMon")				ReadInt(reader, prefs.UseBattMon);
			else if( reader.name() == "DisableMotors")			ReadInt(reader, prefs.DisableMotors);
			else if( reader.name() == "LowVoltageAlarm")		ReadInt(reader, prefs.LowVoltageAlarm);
			else if( reader.name() == "LowVoltageAscentLimit")	ReadInt(reader, prefs.LowVoltageAscentLimit);

			else if( reader.name() == "ThrottleTest")			ReadInt(reader, prefs.ThrottleTest);
			else if( reader.name() == "MinThrottle")			ReadInt(reader, prefs.MinThrottle);
			else if( reader.name() == "MaxThrottle")			ReadInt(reader, prefs.MaxThrottle);
			else if( reader.name() == "CenterThrottle")			ReadInt(reader, prefs.CenterThrottle);
			else if( reader.name() == "MinThrottleArmed")		ReadInt(reader, prefs.MinThrottleArmed);
			else if( reader.name() == "ArmDelay")				ReadInt(reader, prefs.ArmDelay);
			else if( reader.name() == "DisarmDelay")			ReadInt(reader, prefs.DisarmDelay);
			else if( reader.name() == "ThrustCorrectionScale")	ReadInt(reader, prefs.ThrustCorrectionScale);
			else if( reader.name() == "AccelCorrectionFilter")	ReadInt(reader, prefs.AccelCorrectionFilter);
			else if( reader.name() == "VoltageOffset")			ReadInt(reader, prefs.VoltageOffset);
			else if( reader.name() == "LowVoltageAlarmThreshold")	ReadInt(reader, prefs.LowVoltageAlarmThreshold);

			else if( reader.name() == "FlightMode_0")			ReadInt(reader, prefs.FlightMode[0]);
			else if( reader.name() == "FlightMode_1")			ReadInt(reader, prefs.FlightMode[1]);
			else if( reader.name() == "FlightMode_2")			ReadInt(reader, prefs.FlightMode[2]);
			else if( reader.name() == "AccelCorrectionStrength")ReadInt(reader, prefs.AccelCorrectionStrength);

			else if( reader.name() == "AileExpo")				ReadInt(reader, prefs.AileExpo);
			else if( reader.name() == "ElevExpo")				ReadInt(reader, prefs.ElevExpo);
			else if( reader.name() == "RuddExpo")				ReadInt(reader, prefs.RuddExpo);

			else if( reader.name() == "ThroChannel")			ReadInt(reader, prefs.ThroChannel);
			else if( reader.name() == "AileChannel")			ReadInt(reader, prefs.AileChannel);
			else if( reader.name() == "ElevChannel")			ReadInt(reader, prefs.ElevChannel);
			else if( reader.name() == "RuddChannel")			ReadInt(reader, prefs.RuddChannel);
			else if( reader.name() == "GearChannel")			ReadInt(reader, prefs.GearChannel);
			else if( reader.name() == "Aux1Channel")			ReadInt(reader, prefs.Aux1Channel);
			else if( reader.name() == "Aux2Channel")			ReadInt(reader, prefs.Aux2Channel);
			else if( reader.name() == "Aux3Channel")			ReadInt(reader, prefs.Aux3Channel);

			else if( reader.name() == "ThroScale")				ReadInt(reader, prefs.ThroScale);
			else if( reader.name() == "AileScale")				ReadInt(reader, prefs.AileScale);
			else if( reader.name() == "ElevScale")				ReadInt(reader, prefs.ElevScale);
			else if( reader.name() == "RuddScale") 				ReadInt(reader, prefs.RuddScale);
			else if( reader.name() == "GearScale") 				ReadInt(reader, prefs.GearScale);
			else if( reader.name() == "Aux1Scale") 				ReadInt(reader, prefs.Aux1Scale);
			else if( reader.name() == "Aux2Scale") 				ReadInt(reader, prefs.Aux2Scale);
			else if( reader.name() == "Aux3Scale") 				ReadInt(reader, prefs.Aux3Scale);

			else if( reader.name() == "ThroCenter")				ReadInt(reader, prefs.ThroCenter);
			else if( reader.name() == "AileCenter")				ReadInt(reader, prefs.AileCenter);
			else if( reader.name() == "ElevCenter")				ReadInt(reader, prefs.ElevCenter);
			else if( reader.name() == "RuddCenter")				ReadInt(reader, prefs.RuddCenter);
			else if( reader.name() == "GearCenter")				ReadInt(reader, prefs.GearCenter);
			else if( reader.name() == "Aux1Center")				ReadInt(reader, prefs.Aux1Center);
			else if( reader.name() == "Aux2Center")				ReadInt(reader, prefs.Aux2Center);
			else if( reader.name() == "Aux3Center")				ReadInt(reader, prefs.Aux3Center);
		}

		reader.readNext();
	}
}

void MainWindow::on_actionExport_readings_triggered()
{
	// Get the filename from the user
	QString fileName = QFileDialog::getSaveFileName(this, tr("Export sensor readings"), QDir::currentPath(), tr("CSV Files (*.csv)"));
	if (fileName.isEmpty())
		return;

	QFile file(fileName);
	if(!file.open(QFile::WriteOnly | QFile::Text)) {
		return;
	}

	QByteArray bytes;
	QTextStream txt( &bytes );
	QVector<double> heldSamples;

	// Output the column labels first
	int numGraphs = sizeof(graphNames) / sizeof(graphNames[0]);
	for( int i=0; i<numGraphs; i++ ) {
		if( i > 0 ) {
			txt << ",";
		}
		txt << graphNames[i];
		heldSamples.append(0.0);	// "prime" the samples buffer with zero data
	}
	txt << '\n';


	// Iterate through all the samples - this is fun, as there may be holes, IE samples that
	// don't exist.  I have to iterate through the graphs, figure out if there IS a sample for
	// a given index or not, and put it in a table if there is.  Otherwise, the last known good
	// sample is just carried forward until it's replaced.  I could conceivably output NaN for
	// bad samples (or anything non-numeric, really) but this is probably going to be easiest
	// for people to work with.

	for( int i=0; i<SampleIndex; i++ )
	{
		for( int g=0; g<numGraphs; g++)
		{
			if( graphs[g]->data()->contains(i) ){
				heldSamples[g] = (*graphs[g]->data())[i].value;
			}

			if( g > 0 ) txt << ',';
			txt << heldSamples[g];
		}

		txt << '\n';
	}

	txt.flush();

	file.write( bytes );
	file.close();
}
