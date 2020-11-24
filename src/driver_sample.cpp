//============ Copyright (c) Valve Corporation, All rights reserved. ============

#include <openvr_driver.h>
#include <driverlog.h>

#include <vector>
#include <thread>
#include <chrono>
#include <random>

#include <cstring>

#if defined(__GNUC__) || defined(COMPILER_GCC) || defined(__APPLE__)
#define HMD_DLL_EXPORT extern "C" __attribute__((visibility("default")))
#define HMD_DLL_IMPORT extern "C" 
#else
#error "Unsupported Platform."
#endif

inline vr::HmdQuaternion_t HmdQuaternion_Init( double w, double x, double y, double z )
{
	vr::HmdQuaternion_t quat;
	quat.w = w;
	quat.x = x;
	quat.y = y;
	quat.z = z;
	return quat;
}

inline void HmdMatrix_SetIdentity( vr::HmdMatrix34_t *pMatrix )
{
	pMatrix->m[0][0] = 1.f;
	pMatrix->m[0][1] = 0.f;
	pMatrix->m[0][2] = 0.f;
	pMatrix->m[0][3] = 0.f;
	pMatrix->m[1][0] = 0.f;
	pMatrix->m[1][1] = 1.f;
	pMatrix->m[1][2] = 0.f;
	pMatrix->m[1][3] = 0.f;
	pMatrix->m[2][0] = 0.f;
	pMatrix->m[2][1] = 0.f;
	pMatrix->m[2][2] = 1.f;
	pMatrix->m[2][3] = 0.f;
}


// keys for use with the settings API
static const char * const k_pch_Test_Section = "steamvr-test";
static const char * const k_pch_Test_SerialNumber_String = "serialNumber";
static const char * const k_pch_Test_ModelNumber_String = "modelNumber";
static const char * const k_pch_Test_WindowX_Int32 = "windowX";
static const char * const k_pch_Test_WindowY_Int32 = "windowY";
static const char * const k_pch_Test_WindowWidth_Int32 = "windowWidth";
static const char * const k_pch_Test_WindowHeight_Int32 = "windowHeight";
static const char * const k_pch_Test_RenderWidth_Int32 = "renderWidth";
static const char * const k_pch_Test_RenderHeight_Int32 = "renderHeight";
static const char * const k_pch_Test_SecondsFromVsyncToPhotons_Float = "secondsFromVsyncToPhotons";
static const char * const k_pch_Test_DisplayFrequency_Float = "displayFrequency";

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

class CWatchdogDriver_Sample : public vr::IVRWatchdogProvider
{
public:
	CWatchdogDriver_Sample()
	{
		m_pWatchdogThread = nullptr;
	}

	virtual vr::EVRInitError Init( vr::IVRDriverContext *pDriverContext ) ;
	virtual void Cleanup() ;

private:
	std::thread *m_pWatchdogThread;
};

CWatchdogDriver_Sample g_watchdogDriverNull;


bool g_bExiting = false;

void WatchdogThreadFunction(  )
{
	while ( !g_bExiting )
	{
		// for the other platforms, just send one every five seconds
		std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
		vr::VRWatchdogHost()->WatchdogWakeUp( vr::TrackedDeviceClass_HMD );
	}
}

vr::EVRInitError CWatchdogDriver_Sample::Init( vr::IVRDriverContext *pDriverContext )
{
	VR_INIT_WATCHDOG_DRIVER_CONTEXT( pDriverContext );
	InitDriverLog( vr::VRDriverLog() );

	// Watchdog mode on Windows starts a thread that listens for the 'Y' key on the keyboard to 
	// be pressed. A real driver should wait for a system button event or something else from the 
	// the hardware that signals that the VR system should start up.
	g_bExiting = false;
	m_pWatchdogThread = new std::thread( WatchdogThreadFunction );
	if ( !m_pWatchdogThread )
	{
		DriverLog( "Unable to create watchdog thread\n");
		return vr::VRInitError_Driver_Failed;
	}

	return vr::VRInitError_None;
}


void CWatchdogDriver_Sample::Cleanup()
{
	g_bExiting = true;
	if ( m_pWatchdogThread )
	{
		m_pWatchdogThread->join();
		delete m_pWatchdogThread;
		m_pWatchdogThread = nullptr;
	}

	CleanupDriverLog();
}


class CSampleRemoteDisplay : public vr::ITrackedDeviceServerDriver, public vr::IVRVirtualDisplay
{
public:
	CSampleRemoteDisplay( )
	{
		m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
		m_ulPropertyContainer = vr::k_ulInvalidPropertyContainer;

		DriverLog( "Using settings values\n" );
		m_flIPD = vr::VRSettings()->GetFloat( vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_IPD_Float );

		m_sSerialNumber = "TEST_10000000X";
		m_sModelNumber = "TESTNULLHMD";

		//m_nWindowX = vr::VRSettings()->GetInt32( k_pch_Test_Section, k_pch_Test_WindowX_Int32 );
		//m_nWindowY = vr::VRSettings()->GetInt32( k_pch_Test_Section, k_pch_Test_WindowY_Int32 );
		m_nWindowX = m_nWindowY = 0;
		m_nWindowWidth = 1280;
		m_nWindowHeight = 720;
		//m_nWindowWidth = vr::VRSettings()->GetInt32( k_pch_Test_Section, k_pch_Test_WindowWidth_Int32 );
		//m_nWindowHeight = vr::VRSettings()->GetInt32( k_pch_Test_Section, k_pch_Test_WindowHeight_Int32 );
		//m_nRenderWidth = vr::VRSettings()->GetInt32( k_pch_Test_Section, k_pch_Test_RenderWidth_Int32 );
		//m_nRenderHeight = vr::VRSettings()->GetInt32( k_pch_Test_Section, k_pch_Test_RenderHeight_Int32 );
		m_nRenderWidth = m_nWindowWidth;
		m_nRenderHeight = m_nWindowHeight;
		//m_flSecondsFromVsyncToPhotons = vr::VRSettings()->GetFloat( k_pch_Test_Section, k_pch_Test_SecondsFromVsyncToPhotons_Float );
		m_flSecondsFromVsyncToPhotons = 0.0005f;
		//m_flDisplayFrequency = vr::VRSettings()->GetFloat( k_pch_Test_Section, k_pch_Test_DisplayFrequency_Float );
		m_flDisplayFrequency = 90.0f;

		DriverLog( "redirect: Serial Number: %s\n", m_sSerialNumber.c_str() );
		DriverLog( "redirect: Model Number: %s\n", m_sModelNumber.c_str() );
		DriverLog( "redirect: Window: %d %d %d %d\n", m_nWindowX, m_nWindowY, m_nWindowWidth, m_nWindowHeight );
		DriverLog( "redirect: Render Target: %d %d\n", m_nRenderWidth, m_nRenderHeight );
		DriverLog( "redirect: Seconds from Vsync to Photons: %f\n", m_flSecondsFromVsyncToPhotons );
		DriverLog( "redirect: Display Frequency: %f\n", m_flDisplayFrequency );
		DriverLog( "redirect: IPD: %f\n", m_flIPD );

		m_vSyncCounter = 0;
	}

	virtual vr::EVRInitError Activate( uint32_t unObjectId )
	{
		m_unObjectId = unObjectId;
		m_ulPropertyContainer = vr::VRProperties()->TrackedDeviceToPropertyContainer( m_unObjectId );


		vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_ModelNumber_String, m_sModelNumber.c_str() );
		vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_RenderModelName_String, m_sModelNumber.c_str() );
		vr::VRProperties()->SetFloatProperty( m_ulPropertyContainer, vr::Prop_UserIpdMeters_Float, m_flIPD );
		vr::VRProperties()->SetFloatProperty( m_ulPropertyContainer, vr::Prop_UserHeadToEyeDepthMeters_Float, 0.f );
		vr::VRProperties()->SetFloatProperty( m_ulPropertyContainer, vr::Prop_DisplayFrequency_Float, m_flDisplayFrequency );
		vr::VRProperties()->SetFloatProperty( m_ulPropertyContainer, vr::Prop_SecondsFromVsyncToPhotons_Float, m_flSecondsFromVsyncToPhotons );

		// return a constant that's not 0 (invalid) or 1 (reserved for Oculus)
		vr::VRProperties()->SetUint64Property( m_ulPropertyContainer, vr::Prop_CurrentUniverseId_Uint64, 2 );

		// avoid "not fullscreen" warnings from vrmonitor
		vr::VRProperties()->SetBoolProperty( m_ulPropertyContainer, vr::Prop_IsOnDesktop_Bool, false );

		// Icons can be configured in code or automatically configured by an external file "drivername\resources\driver.vrresources".
		// Icon properties NOT configured in code (post Activate) are then auto-configured by the optional presence of a driver's "drivername\resources\driver.vrresources".
		// In this manner a driver can configure their icons in a flexible data driven fashion by using an external file.
		//
		// The structure of the driver.vrresources file allows a driver to specialize their icons based on their HW.
		// Keys matching the value in "Prop_ModelNumber_String" are considered first, since the driver may have model specific icons.
		// An absence of a matching "Prop_ModelNumber_String" then considers the ETrackedDeviceClass ("HMD", "Controller", "GenericTracker", "TrackingReference")
		// since the driver may have specialized icons based on those device class names.
		//
		// An absence of either then falls back to the "system.vrresources" where generic device class icons are then supplied.
		//
		// Please refer to "bin\drivers\sample\resources\driver.vrresources" which contains this sample configuration.
		//
		// "Alias" is a reserved key and specifies chaining to another json block.
		//
		// In this sample configuration file (overly complex FOR EXAMPLE PURPOSES ONLY)....
		//
		// "Model-v2.0" chains through the alias to "Model-v1.0" which chains through the alias to "Model-v Defaults".
		//
		// Keys NOT found in "Model-v2.0" would then chase through the "Alias" to be resolved in "Model-v1.0" and either resolve their or continue through the alias.
		// Thus "Prop_NamedIconPathDeviceAlertLow_String" in each model's block represent a specialization specific for that "model".
		// Keys in "Model-v Defaults" are an example of mapping to the same states, and here all map to "Prop_NamedIconPathDeviceOff_String".
		//
		bool bSetupIconUsingExternalResourceFile = true;
		if ( !bSetupIconUsingExternalResourceFile )
		{
			// Setup properties directly in code.
			// Path values are of the form {drivername}\icons\some_icon_filename.png
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceOff_String, "{sample}/icons/headset_sample_status_off.png" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearching_String, "{sample}/icons/headset_sample_status_searching.gif" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{sample}/icons/headset_sample_status_searching_alert.gif" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReady_String, "{sample}/icons/headset_sample_status_ready.png" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{sample}/icons/headset_sample_status_ready_alert.png" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceNotReady_String, "{sample}/icons/headset_sample_status_error.png" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceStandby_String, "{sample}/icons/headset_sample_status_standby.png" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceAlertLow_String, "{sample}/icons/headset_sample_status_ready_low.png" );
		}

		DriverLog("Activating virtual display!\n");

		return vr::VRInitError_None;
	}

	virtual void Deactivate()
	{

	}

	virtual void EnterStandby()
	{
		DriverLog("Virtual display STANDBY\n");
	}

	virtual void *GetComponent( const char *pchComponentNameAndVersion )
	{
		if ( !strcmp(pchComponentNameAndVersion, vr::IVRVirtualDisplay_Version) )
		{
			DriverLog("###### Requested VirtualDisplay! Returning this...\n");
			return (vr::IVRVirtualDisplay*)this;
		}

		// override this to add a component to a driver
		return NULL;
	}

	virtual void DebugRequest( const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize )
	{
		if ( unResponseBufferSize >= 1 )
			pchResponseBuffer[0] = 0;
	}

	virtual vr::DriverPose_t GetPose()
	{
		vr::DriverPose_t pose = { 0 };
		pose.poseIsValid = true;
		pose.result = vr::TrackingResult_Running_OK;
		pose.deviceIsConnected = true;
		pose.qWorldFromDriverRotation.w = 1;
		pose.qWorldFromDriverRotation.x = 0;
		pose.qWorldFromDriverRotation.y = 0;
		pose.qWorldFromDriverRotation.z = 0;
		pose.qDriverFromHeadRotation.w = 1;
		pose.qDriverFromHeadRotation.x = 0;
		pose.qDriverFromHeadRotation.y = 0;
		pose.qDriverFromHeadRotation.z = 0;
		return pose;
	}



	/** Submits final backbuffer for display. */
	virtual void Present( const vr::PresentInfo_t *pPresentInfo, uint32_t unPresentInfoSize )
	{
		DriverLog("########## Presenting!! ###########\n");
		m_vSyncCounter++;
		return;
	}

	/** Block until the last presented buffer start scanning out. */
	virtual void WaitForPresent()
	{
		DriverLog("Waiting for 5ms...\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		return;
	}

	/** Provides timing data for synchronizing with display. */
	virtual bool GetTimeSinceLastVsync( float *pfSecondsSinceLastVsync, uint64_t *pulFrameCounter )
	{
		auto currentTime = std::chrono::system_clock::now();
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch());
		
		*pfSecondsSinceLastVsync = millis.count() % 11 / 1000.0f;
		*pulFrameCounter = m_vSyncCounter;
		DriverLog("Reporting time since last VSync: %f\n", *pfSecondsSinceLastVsync);
		return true;
	}

	std::string GetSerialNumber() const { return m_sSerialNumber; }

private:
	vr::TrackedDeviceIndex_t m_unObjectId;
	vr::PropertyContainerHandle_t m_ulPropertyContainer;

	std::string m_sSerialNumber;
	std::string m_sModelNumber;

	int32_t m_nWindowX;
	int32_t m_nWindowY;
	int32_t m_nWindowWidth;
	int32_t m_nWindowHeight;
	int32_t m_nRenderWidth;
	int32_t m_nRenderHeight;
	float m_flSecondsFromVsyncToPhotons;
	float m_flDisplayFrequency;
	float m_flIPD;

	uint64_t m_vSyncCounter;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSampleDeviceDriver : public vr::ITrackedDeviceServerDriver, public vr::IVRDisplayComponent
{
public:
	CSampleDeviceDriver(  )
	{
		m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
		m_ulPropertyContainer = vr::k_ulInvalidPropertyContainer;

		DriverLog( "Using settings values\n" );
		m_flIPD = vr::VRSettings()->GetFloat( vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_IPD_Float );

		m_sSerialNumber = "TEST_10000001X";
		m_sModelNumber = "TESTNULLHMD";

		//m_nWindowX = vr::VRSettings()->GetInt32( k_pch_Test_Section, k_pch_Test_WindowX_Int32 );
		//m_nWindowY = vr::VRSettings()->GetInt32( k_pch_Test_Section, k_pch_Test_WindowY_Int32 );
		m_nWindowX = m_nWindowY = 0;
		m_nWindowWidth = 1280;
		m_nWindowHeight = 720;
		//m_nWindowWidth = vr::VRSettings()->GetInt32( k_pch_Test_Section, k_pch_Test_WindowWidth_Int32 );
		//m_nWindowHeight = vr::VRSettings()->GetInt32( k_pch_Test_Section, k_pch_Test_WindowHeight_Int32 );
		//m_nRenderWidth = vr::VRSettings()->GetInt32( k_pch_Test_Section, k_pch_Test_RenderWidth_Int32 );
		//m_nRenderHeight = vr::VRSettings()->GetInt32( k_pch_Test_Section, k_pch_Test_RenderHeight_Int32 );
		m_nRenderWidth = m_nWindowWidth;
		m_nRenderHeight = m_nWindowHeight;
		//m_flSecondsFromVsyncToPhotons = vr::VRSettings()->GetFloat( k_pch_Test_Section, k_pch_Test_SecondsFromVsyncToPhotons_Float );
		m_flSecondsFromVsyncToPhotons = 0.0005f;
		//m_flDisplayFrequency = vr::VRSettings()->GetFloat( k_pch_Test_Section, k_pch_Test_DisplayFrequency_Float );
		m_flDisplayFrequency = 90.0f;

		DriverLog( "driver_null: Serial Number: %s\n", m_sSerialNumber.c_str() );
		DriverLog( "driver_null: Model Number: %s\n", m_sModelNumber.c_str() );
		DriverLog( "driver_null: Window: %d %d %d %d\n", m_nWindowX, m_nWindowY, m_nWindowWidth, m_nWindowHeight );
		DriverLog( "driver_null: Render Target: %d %d\n", m_nRenderWidth, m_nRenderHeight );
		DriverLog( "driver_null: Seconds from Vsync to Photons: %f\n", m_flSecondsFromVsyncToPhotons );
		DriverLog( "driver_null: Display Frequency: %f\n", m_flDisplayFrequency );
		DriverLog( "driver_null: IPD: %f\n", m_flIPD );
	}

	virtual ~CSampleDeviceDriver()
	{
	}


	virtual vr::EVRInitError Activate( vr::TrackedDeviceIndex_t unObjectId ) 
	{
		m_unObjectId = unObjectId;
		m_ulPropertyContainer = vr::VRProperties()->TrackedDeviceToPropertyContainer( m_unObjectId );


		vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_ModelNumber_String, m_sModelNumber.c_str() );
		vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_RenderModelName_String, m_sModelNumber.c_str() );
		vr::VRProperties()->SetFloatProperty( m_ulPropertyContainer, vr::Prop_UserIpdMeters_Float, m_flIPD );
		vr::VRProperties()->SetFloatProperty( m_ulPropertyContainer, vr::Prop_UserHeadToEyeDepthMeters_Float, 0.f );
		vr::VRProperties()->SetFloatProperty( m_ulPropertyContainer, vr::Prop_DisplayFrequency_Float, m_flDisplayFrequency );
		vr::VRProperties()->SetFloatProperty( m_ulPropertyContainer, vr::Prop_SecondsFromVsyncToPhotons_Float, m_flSecondsFromVsyncToPhotons );

		// return a constant that's not 0 (invalid) or 1 (reserved for Oculus)
		vr::VRProperties()->SetUint64Property( m_ulPropertyContainer, vr::Prop_CurrentUniverseId_Uint64, 2 );

		// avoid "not fullscreen" warnings from vrmonitor
		vr::VRProperties()->SetBoolProperty( m_ulPropertyContainer, vr::Prop_IsOnDesktop_Bool, false );

		// Icons can be configured in code or automatically configured by an external file "drivername\resources\driver.vrresources".
		// Icon properties NOT configured in code (post Activate) are then auto-configured by the optional presence of a driver's "drivername\resources\driver.vrresources".
		// In this manner a driver can configure their icons in a flexible data driven fashion by using an external file.
		//
		// The structure of the driver.vrresources file allows a driver to specialize their icons based on their HW.
		// Keys matching the value in "Prop_ModelNumber_String" are considered first, since the driver may have model specific icons.
		// An absence of a matching "Prop_ModelNumber_String" then considers the ETrackedDeviceClass ("HMD", "Controller", "GenericTracker", "TrackingReference")
		// since the driver may have specialized icons based on those device class names.
		//
		// An absence of either then falls back to the "system.vrresources" where generic device class icons are then supplied.
		//
		// Please refer to "bin\drivers\sample\resources\driver.vrresources" which contains this sample configuration.
		//
		// "Alias" is a reserved key and specifies chaining to another json block.
		//
		// In this sample configuration file (overly complex FOR EXAMPLE PURPOSES ONLY)....
		//
		// "Model-v2.0" chains through the alias to "Model-v1.0" which chains through the alias to "Model-v Defaults".
		//
		// Keys NOT found in "Model-v2.0" would then chase through the "Alias" to be resolved in "Model-v1.0" and either resolve their or continue through the alias.
		// Thus "Prop_NamedIconPathDeviceAlertLow_String" in each model's block represent a specialization specific for that "model".
		// Keys in "Model-v Defaults" are an example of mapping to the same states, and here all map to "Prop_NamedIconPathDeviceOff_String".
		//
		bool bSetupIconUsingExternalResourceFile = true;
		if ( !bSetupIconUsingExternalResourceFile )
		{
			// Setup properties directly in code.
			// Path values are of the form {drivername}\icons\some_icon_filename.png
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceOff_String, "{sample}/icons/headset_sample_status_off.png" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearching_String, "{sample}/icons/headset_sample_status_searching.gif" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{sample}/icons/headset_sample_status_searching_alert.gif" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReady_String, "{sample}/icons/headset_sample_status_ready.png" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{sample}/icons/headset_sample_status_ready_alert.png" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceNotReady_String, "{sample}/icons/headset_sample_status_error.png" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceStandby_String, "{sample}/icons/headset_sample_status_standby.png" );
			vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceAlertLow_String, "{sample}/icons/headset_sample_status_ready_low.png" );
		}

		srand(0);

		return vr::VRInitError_None;
	}

	virtual void Deactivate() 
	{
		m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
	}

	virtual void EnterStandby()
	{
	}

	void *GetComponent( const char *pchComponentNameAndVersion )
	{
		if ( !strcmp( pchComponentNameAndVersion, vr::IVRDisplayComponent_Version ) )
		{
			return (vr::IVRDisplayComponent*)this;
		}

		// override this to add a component to a driver
		return NULL;
	}

	virtual void PowerOff() 
	{
	}

	/** debug request from a client */
	virtual void DebugRequest( const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize ) 
	{
		if( unResponseBufferSize >= 1 )
			pchResponseBuffer[0] = 0;
	}

	virtual void GetWindowBounds( int32_t *pnX, int32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight ) 
	{
		*pnX = m_nWindowX;
		*pnY = m_nWindowY;
		*pnWidth = m_nWindowWidth;
		*pnHeight = m_nWindowHeight;
	}

	virtual bool IsDisplayOnDesktop() 
	{
		return true;
	}

	virtual bool IsDisplayRealDisplay() 
	{
		return false;
	}

	virtual void GetRecommendedRenderTargetSize( uint32_t *pnWidth, uint32_t *pnHeight ) 
	{
		*pnWidth = m_nRenderWidth;
		*pnHeight = m_nRenderHeight;
	}

	virtual void GetEyeOutputViewport( vr::EVREye eEye, uint32_t *pnX, uint32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight ) 
	{
		*pnY = 0;
		*pnWidth = m_nWindowWidth / 2;
		*pnHeight = m_nWindowHeight;
	
		if ( eEye == vr::Eye_Left )
		{
			*pnX = 0;
		}
		else
		{
			*pnX = m_nWindowWidth / 2;
		}
	}

	virtual void GetProjectionRaw( vr::EVREye eEye, float *pfLeft, float *pfRight, float *pfTop, float *pfBottom ) 
	{
		*pfLeft = -1.0;
		*pfRight = 1.0;
		*pfTop = -1.0;
		*pfBottom = 1.0;	
	}

	virtual vr::DistortionCoordinates_t ComputeDistortion( vr::EVREye eEye, float fU, float fV ) 
	{
		vr::DistortionCoordinates_t coordinates;
		coordinates.rfBlue[0] = fU;
		coordinates.rfBlue[1] = fV;
		coordinates.rfGreen[0] = fU;
		coordinates.rfGreen[1] = fV;
		coordinates.rfRed[0] = fU;
		coordinates.rfRed[1] = fV;
		return coordinates;
	}

	virtual vr::DriverPose_t GetPose() 
	{
		vr::DriverPose_t pose = { 0 };
		pose.poseIsValid = true;
		pose.result = vr::TrackingResult_Running_OK;
		pose.deviceIsConnected = true;

		pose.qWorldFromDriverRotation = HmdQuaternion_Init( 1, 0, 0, 0 );
		pose.qDriverFromHeadRotation = HmdQuaternion_Init( 1, 0, 0, 0 );
		pose.vecPosition[0] = 0.0;
		pose.vecPosition[1] = double(rand() % 500) / 100000.0;
		pose.vecPosition[2] = 0.0;
		

		return pose;
	}
	

	void RunFrame()
	{
		// In a real driver, this should happen from some pose tracking thread.
		// The RunFrame interval is unspecified and can be very irregular if some other
		// driver blocks it for some periodic task.
		if ( m_unObjectId != vr::k_unTrackedDeviceIndexInvalid )
		{
			vr::VRServerDriverHost()->TrackedDevicePoseUpdated( m_unObjectId, GetPose(), sizeof( vr::DriverPose_t ) );
		}
	}

	std::string GetSerialNumber() const { return m_sSerialNumber; }

private:
	vr::TrackedDeviceIndex_t m_unObjectId;
	vr::PropertyContainerHandle_t m_ulPropertyContainer;

	std::string m_sSerialNumber;
	std::string m_sModelNumber;

	int32_t m_nWindowX;
	int32_t m_nWindowY;
	int32_t m_nWindowWidth;
	int32_t m_nWindowHeight;
	int32_t m_nRenderWidth;
	int32_t m_nRenderHeight;
	float m_flSecondsFromVsyncToPhotons;
	float m_flDisplayFrequency;
	float m_flIPD;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSampleControllerDriver : public vr::ITrackedDeviceServerDriver
{
public:
	CSampleControllerDriver()
	{
		m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
		m_ulPropertyContainer = vr::k_ulInvalidPropertyContainer;

		m_sSerialNumber = "CTRL_1234";

		m_sModelNumber = "MyController";
	}

	virtual ~CSampleControllerDriver()
	{
	}


	virtual vr::EVRInitError Activate( vr::TrackedDeviceIndex_t unObjectId )
	{
		m_unObjectId = unObjectId;
		m_ulPropertyContainer = vr::VRProperties()->TrackedDeviceToPropertyContainer( m_unObjectId );

		vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_ModelNumber_String, m_sModelNumber.c_str() );
		vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_RenderModelName_String, m_sModelNumber.c_str() );

		// return a constant that's not 0 (invalid) or 1 (reserved for Oculus)
		vr::VRProperties()->SetUint64Property( m_ulPropertyContainer, vr::Prop_CurrentUniverseId_Uint64, 2 );

		// avoid "not fullscreen" warnings from vrmonitor
		vr::VRProperties()->SetBoolProperty( m_ulPropertyContainer, vr::Prop_IsOnDesktop_Bool, false );

		// our sample device isn't actually tracked, so set this property to avoid having the icon blink in the status window
		vr::VRProperties()->SetBoolProperty( m_ulPropertyContainer, vr::Prop_NeverTracked_Bool, true );

		// even though we won't ever track we want to pretend to be the right hand so binding will work as expected
		vr::VRProperties()->SetInt32Property( m_ulPropertyContainer, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_RightHand );

		// this file tells the UI what to show the user for binding this controller as well as what default bindings should
		// be for legacy or other apps
		vr::VRProperties()->SetStringProperty( m_ulPropertyContainer, vr::Prop_InputProfilePath_String, "{sample}/input/mycontroller_profile.json" );

		// create all the input components
		vr::VRDriverInput()->CreateBooleanComponent( m_ulPropertyContainer, "/input/a/click", &m_compA );
		vr::VRDriverInput()->CreateBooleanComponent( m_ulPropertyContainer, "/input/b/click", &m_compB );
		vr::VRDriverInput()->CreateBooleanComponent( m_ulPropertyContainer, "/input/c/click", &m_compC );

		// create our haptic component
		vr::VRDriverInput()->CreateHapticComponent( m_ulPropertyContainer, "/output/haptic", &m_compHaptic );

		return vr::VRInitError_None;
	}

	virtual void Deactivate()
	{
		m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
	}

	virtual void EnterStandby()
	{
	}

	void *GetComponent( const char *pchComponentNameAndVersion )
	{
		// override this to add a component to a driver
		return NULL;
	}

	virtual void PowerOff()
	{
	}

	/** debug request from a client */
	virtual void DebugRequest( const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize )
	{
		if ( unResponseBufferSize >= 1 )
			pchResponseBuffer[0] = 0;
	}

	virtual vr::DriverPose_t GetPose()
	{
		vr::DriverPose_t pose = { 0 };
		pose.poseIsValid = false;
		pose.result = vr::TrackingResult_Calibrating_OutOfRange;
		pose.deviceIsConnected = true;

		pose.qWorldFromDriverRotation = HmdQuaternion_Init( 1, 0, 0, 0 );
		pose.qDriverFromHeadRotation = HmdQuaternion_Init( 1, 0, 0, 0 );

		return pose;
	}


	void RunFrame()
	{
#if defined( _WINDOWS )
		// Your driver would read whatever hardware state is associated with its input components and pass that
		// in to UpdateBooleanComponent. This could happen in RunFrame or on a thread of your own that's reading USB
		// state. There's no need to update input state unless it changes, but it doesn't do any harm to do so.

		vr::VRDriverInput()->UpdateBooleanComponent( m_compA, (0x8000 & GetAsyncKeyState( 'A' )) != 0, 0 );
		vr::VRDriverInput()->UpdateBooleanComponent( m_compB, (0x8000 & GetAsyncKeyState( 'B' )) != 0, 0 );
		vr::VRDriverInput()->UpdateBooleanComponent( m_compC, (0x8000 & GetAsyncKeyState( 'C' )) != 0, 0 );
#endif
	}

	void ProcessEvent( const vr::VREvent_t & vrEvent )
	{
		switch ( vrEvent.eventType )
		{
		case vr::VREvent_Input_HapticVibration:
		{
			if ( vrEvent.data.hapticVibration.componentHandle == m_compHaptic )
			{
				// This is where you would send a signal to your hardware to trigger actual haptic feedback
				DriverLog( "BUZZ!\n" );
			}
		}
		break;
		}
	}


	std::string GetSerialNumber() const { return m_sSerialNumber; }

private:
	vr::TrackedDeviceIndex_t m_unObjectId;
	vr::PropertyContainerHandle_t m_ulPropertyContainer;

	vr::VRInputComponentHandle_t m_compA;
	vr::VRInputComponentHandle_t m_compB;
	vr::VRInputComponentHandle_t m_compC;
	vr::VRInputComponentHandle_t m_compHaptic;

	std::string m_sSerialNumber;
	std::string m_sModelNumber;


};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CServerDriver_Sample: public vr::IServerTrackedDeviceProvider
{
public:
	virtual vr::EVRInitError Init( vr::IVRDriverContext *pDriverContext ) ;
	virtual void Cleanup() ;
	virtual const char * const *GetInterfaceVersions() { return vr::k_InterfaceVersions; }
	virtual void RunFrame() ;
	virtual bool ShouldBlockStandbyMode()  { return false; }
	virtual void EnterStandby()  {}
	virtual void LeaveStandby()  {}

private:
	CSampleDeviceDriver *m_pNullHmdLatest = nullptr;
	CSampleControllerDriver *m_pController = nullptr;
	CSampleRemoteDisplay *m_pRemodeDisplay = nullptr;
};

CServerDriver_Sample g_serverDriverNull;


vr::EVRInitError CServerDriver_Sample::Init( vr::IVRDriverContext *pDriverContext )
{
	VR_INIT_SERVER_DRIVER_CONTEXT( pDriverContext );
	InitDriverLog( vr::VRDriverLog() );


	m_pNullHmdLatest = new CSampleDeviceDriver();
	vr::VRServerDriverHost()->TrackedDeviceAdded( m_pNullHmdLatest->GetSerialNumber().c_str(), vr::TrackedDeviceClass_HMD, m_pNullHmdLatest );

	m_pRemodeDisplay = new CSampleRemoteDisplay();
	vr::VRServerDriverHost()->TrackedDeviceAdded( m_pRemodeDisplay->GetSerialNumber().c_str(), vr::TrackedDeviceClass_DisplayRedirect, m_pRemodeDisplay );

	m_pController = new CSampleControllerDriver();
	vr::VRServerDriverHost()->TrackedDeviceAdded( m_pController->GetSerialNumber().c_str(), vr::TrackedDeviceClass_Controller, m_pController );


	return vr::VRInitError_None;
}

void CServerDriver_Sample::Cleanup() 
{
	CleanupDriverLog();
	delete m_pNullHmdLatest;
	m_pNullHmdLatest = NULL;
	delete m_pController;
	m_pController = NULL;
	delete m_pRemodeDisplay;
	m_pRemodeDisplay = NULL;
}


void CServerDriver_Sample::RunFrame()
{
	if ( m_pNullHmdLatest )
	{
		m_pNullHmdLatest->RunFrame();
	}
	if ( m_pController )
	{
		m_pController->RunFrame();
	}

	vr::VREvent_t vrEvent;
	while ( vr::VRServerDriverHost()->PollNextEvent( &vrEvent, sizeof( vrEvent ) ) )
	{
		if ( m_pController )
		{
			m_pController->ProcessEvent( vrEvent );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
HMD_DLL_EXPORT void *HmdDriverFactory( const char *pInterfaceName, int *pReturnCode )
{
	if( 0 == strcmp( vr::IServerTrackedDeviceProvider_Version, pInterfaceName ) )
	{
		return &g_serverDriverNull;
	}
	if( 0 == strcmp( vr::IVRWatchdogProvider_Version, pInterfaceName ) )
	{
		return &g_watchdogDriverNull;
	}

	if( pReturnCode )
		*pReturnCode = vr::VRInitError_Init_InterfaceNotFound;

	return NULL;
}
