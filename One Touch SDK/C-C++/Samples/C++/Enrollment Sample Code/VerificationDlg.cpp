// VerificationDlg.cpp : implementation file
//
#include "stdafx.h"
#include "VerificationDlg.h"
#include "Utilities.h"
#include "dpFtrEx.h"
#include "dpMatch.h"

/**************************************************************************************************
                                Sample code for Fingerprint Verification
                                Copyright Digital Persona, Inc. 1996-2007
/*************************************************************************************************/

/////////////////////////////////////////////////////////////////////////////
// CVerificationDlg dialog

CVerificationDlg::CVerificationDlg() : 
	CDialogImpl<CVerificationDlg>(), 
	m_bDoLearning(FT_FALSE),
	m_hOperationVerify(0),
	m_fxContext(0),
	m_mcContext(0)
{
	::ZeroMemory(&m_rcDrawArea, sizeof(m_rcDrawArea));
	::ZeroMemory(&m_RegTemplate, sizeof(m_RegTemplate));
}

CVerificationDlg::~CVerificationDlg() {
	delete [] m_RegTemplate.pbData;
	m_RegTemplate.cbData = 0;
	m_RegTemplate.pbData = NULL;
}

void CVerificationDlg::LoadRegTemplate(const DATA_BLOB& rRegTemplate) {
	// Delete the old stuff that may be in the template.
	delete [] m_RegTemplate.pbData;
	m_RegTemplate.pbData = NULL;
	m_RegTemplate.cbData = 0;

	// Copy Enrollment template data into member of this class
	m_RegTemplate.pbData = new BYTE[rRegTemplate.cbData];
	if (!m_RegTemplate.pbData) _com_issue_error(E_OUTOFMEMORY);
	::CopyMemory(m_RegTemplate.pbData, rRegTemplate.pbData, rRegTemplate.cbData);
	m_RegTemplate.cbData = rRegTemplate.cbData;
}

void CVerificationDlg::AddStatus(LPCTSTR s) {
	int lIdx = SendDlgItemMessage(IDC_STATUS, LB_ADDSTRING, 0, (LPARAM)s);
	SendDlgItemMessage(IDC_STATUS, LB_SETTOPINDEX, lIdx, 0);
}

LRESULT CVerificationDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	HRESULT hr = S_OK;
	try {
		FT_RETCODE rc = FT_OK;

		// Create Context for Feature Extraction
		if (FT_OK != (rc = FX_createContext(&m_fxContext))) {
			MessageBox(_T("Cannot create Feature Extraction Context."), _T("Fingerprint Verification"), MB_OK | MB_ICONSTOP);
			EndDialog(IDCANCEL);
			return TRUE;  // return TRUE  unless you set the focus to a control
		}

		// Create Context for Matching
		if (FT_OK != (rc = MC_createContext(&m_mcContext))) {
			MessageBox(_T("Cannot create Matching Context."), _T("Fingerprint Verification"), MB_OK | MB_ICONSTOP);
			EndDialog(IDCANCEL);
			return TRUE;  // return TRUE  unless you set the focus to a control
		}

		::GetWindowRect(GetDlgItem(IDC_STATIC_DRAWAREA_SIZE), &m_rcDrawArea);

		// Start Verification.
		DP_ACQUISITION_PRIORITY ePriority = DP_PRIORITY_NORMAL; // Using Normal Priority, i.e. fingerprint will be sent to 
											  // this process only if it has active window on the desktop.
		_com_test_error(DPFPCreateAcquisition(ePriority, GUID_NULL, DP_SAMPLE_TYPE_IMAGE, m_hWnd, WMUS_FP_NOTIFY, &m_hOperationVerify));
		_com_test_error(DPFPStartAcquisition(m_hOperationVerify));

		SetDlgItemText(IDC_EDIT_PROMPT, _T("Scan your finger for verification."));
	}
	catch(_com_error& E) {
		hr = E.Error();
	}
	catch(...) {
		hr = E_UNEXPECTED;
	}

	if (FAILED(hr)) {
		SetDlgItemText(IDC_EDIT_PROMPT, _T("Error happened"));
		ReportError(m_hWnd, hr);
		EndDialog(IDCANCEL);
	}

	return TRUE;  // return TRUE  unless you set the focus to a control
}

LRESULT CVerificationDlg::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (m_hOperationVerify) {
		DPFPStopAcquisition(m_hOperationVerify);    // No error checking - what can we do at the end anyway?
		DPFPDestroyAcquisition(m_hOperationVerify);
		m_hOperationVerify = 0;
	}

	if (m_fxContext) {
		FX_closeContext(m_fxContext);
		m_fxContext = 0;
	}

	if (m_mcContext) {
		MC_closeContext(m_mcContext);
		m_mcContext = 0;
	}

	return 0;
}

LRESULT CVerificationDlg::OnFpNotify(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
	switch(wParam) {
		case WN_COMPLETED: {
	        AddStatus(_T("Fingerprint image captured"));

			DATA_BLOB* pImageBlob = reinterpret_cast<DATA_BLOB*>(lParam);

			// Display fingerprint image
			DisplayFingerprintImage(pImageBlob);//->pbData);

			// Match the new fingerprint image and Enrollment template
			Verify(pImageBlob->pbData, pImageBlob->cbData);
			break;
		}
		case WN_ERROR: {
			TCHAR buffer[101] = {0};
			_sntprintf_s(buffer, 100, _TRUNCATE, _T("Error happened. Error code: 0x%X"), lParam);
	        AddStatus(buffer);
			break;
		}
		case WN_DISCONNECT:
	        AddStatus(_T("Fingerprint reader disconnected"));
			break;
		case WN_RECONNECT:
	        AddStatus(_T("Fingerprint reader connected"));
			break;
		case WN_FINGER_TOUCHED:
	        AddStatus(_T("Finger touched"));
			break;
		case WN_FINGER_GONE:
	        AddStatus(_T("Finger gone"));
			break;
		case WN_IMAGE_READY:
	        AddStatus(_T("Fingerprint image ready"));
			break;
		case WN_OPERATION_STOPPED:
	        AddStatus(_T("Fingerprint Verification Operation stopped"));
			break;
	}
	return 0;
}

LRESULT CVerificationDlg::OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	EndDialog(wID);
	return 0;
}
//#include "DPFPApi.h"
DPFP_STDAPI DPFPConvertSampleToBitmap(const DATA_BLOB* pSample, BYTE* pBitmap, size_t* pSize);
void CVerificationDlg::DisplayFingerprintImage(const DATA_BLOB* pImageBlob)//FT_IMAGE_PT pFingerprintImage)
{
	HRESULT hr = S_OK;
	PBITMAPINFO pOutBmp = NULL;
	try {
		size_t Size = 0;
		hr = DPFPConvertSampleToBitmap(pImageBlob, 0, &Size);
		pOutBmp = (PBITMAPINFO)new BYTE[Size];
		hr = DPFPConvertSampleToBitmap(pImageBlob, (PBYTE)pOutBmp, &Size);

		LONG WidthOut  = m_rcDrawArea.right  - m_rcDrawArea.left;
		LONG HeightOut = m_rcDrawArea.bottom - m_rcDrawArea.top;

		if (SUCCEEDED(hr)) {
			size_t dwColorsSize = pOutBmp->bmiHeader.biClrUsed * sizeof(PALETTEENTRY);
			const BYTE* pBmpBits = (PBYTE)pOutBmp + sizeof(BITMAPINFOHEADER) + dwColorsSize;
			// Create bitmap and set its handle to the control for display.
			HDC hdcScreen = GetDC();
			HDC hdcMem = CreateCompatibleDC(hdcScreen);
/*
			LPBYTE dstBits = NULL;
			HBITMAP hBmp1 = CreateDIBSection(0, pOutBmp, DIB_RGB_COLORS, (void**)&dstBits, 0, 0);
			memcpy(dstBits, pBmpBits, pOutBmp->bmiHeader.biSizeImage);
			SelectObject(hdcMem, hBmp1);
			int k = BitBlt(hdcScreen, 0, 0, pOutBmp->bmiHeader.biWidth, pOutBmp->bmiHeader.biHeight, hdcMem, 0, 0, SRCCOPY);
*/
			HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, WidthOut, HeightOut);
			SelectObject(hdcMem, hBmp);

			int i = StretchDIBits(hdcMem, 0, 0, WidthOut, HeightOut, 0, 0, pOutBmp->bmiHeader.biWidth, pOutBmp->bmiHeader.biHeight, pBmpBits, pOutBmp, DIB_RGB_COLORS, SRCCOPY);
			//int j = BitBlt(hdcScreen, 0, 0, WidthOut, HeightOut, hdcMem, 0, 0, SRCCOPY);
			HBITMAP hOldBmp = reinterpret_cast<HBITMAP>(SendDlgItemMessage(IDC_STATIC_DRAWAREA, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp));
			if (hOldBmp)
				DeleteObject(hOldBmp);

			DeleteDC(hdcMem);
			ReleaseDC(hdcScreen);
			InvalidateRect(NULL, TRUE);
		}
		delete [] (PBYTE)pOutBmp;
	}
	catch(_com_error E) {
		hr = E.Error();
	}
	catch(...) {
		hr = E_UNEXPECTED;
	}

	if (FAILED(hr))
		ReportError(m_hWnd, hr);
}

void CVerificationDlg::Verify(FT_IMAGE_PT pFingerprintImage, int iFingerprintImageSize) {
	HRESULT hr = S_OK;
	FT_BYTE* pVerTemplate = NULL;
	try {
		FT_RETCODE rc = FT_OK;

		// Get recommended length of the Pre-Enrollment feature sets.
		// It actually could be done once at the beginning, but this is just sample code...
		int iRecommendedVerFtrLen = 0;
		rc = FX_getFeaturesLen(FT_VER_FTR,
		                       &iRecommendedVerFtrLen,
		                       NULL); // Minimal Fingerprint feature set Length - do not use if possible
		if (FT_OK != rc) _com_issue_error(E_FAIL);

		// Extract Features (i.e. create fingerprint template)
		FT_IMG_QUALITY imgQuality;
		FT_FTR_QUALITY ftrQuality;
		FT_BOOL bEextractOK = FT_FALSE;

		if (NULL == (pVerTemplate = new FT_BYTE[iRecommendedVerFtrLen])) _com_issue_error(E_OUTOFMEMORY);
		rc = FX_extractFeatures(m_fxContext,             // Context of Feature Extraction
		                        iFingerprintImageSize,   // Size of the fingerprint image
		                        pFingerprintImage,       // Pointer to the image buffer
		                        FT_VER_FTR,              // Requested Verification Features
		                        iRecommendedVerFtrLen,   // Length of the Features(template) buffer received previously from the SDK
		                        pVerTemplate,            // Pointer to the buffer where the template to be stored
		                        &imgQuality,             // Returned quality of the image. If feature extraction fails because used did not put finger on the reader well enough, this parameter be used to tell the user how to put the finger on the reader better. See FT_IMG_QUALITY enumeration.
		                        &ftrQuality,             // Returned quality of the features. It may happen that the fingerprint of the user cannot be used. See FT_FTR_QUALITY enumeration.
		                        &bEextractOK);           // Returned result of Feature extraction. FT_TRUE means extracted OK.

		if (FT_OK <= rc && bEextractOK == FT_TRUE) {
			// Features extracted OK (i.e. fingerprint Verification template was created successfully)
			// Now match this template and the Enrollment template.
			FT_BOOL bRegFeaturesChanged = FT_FALSE;
			FT_VER_SCORE VerScore = FT_UNKNOWN_SCORE;
			double dFalseAcceptProbability = 0.0;
			FT_BOOL bVerified = FT_FALSE;

			rc = MC_verifyFeaturesEx(m_mcContext,              // Matching Context
			                         m_RegTemplate.cbData,     // Pointer to the Enrollment template content
			                         m_RegTemplate.pbData,     // Length of the Enrollment template
			                         iRecommendedVerFtrLen,    // Length of the Verification template
			                         pVerTemplate,             // Pointer to the Verification template content
			                         m_bDoLearning,            // Whether the Learning is desired - got it from checkbox in the dialog
			                         &bRegFeaturesChanged,     // Out: Whether the Learning actually happened
			                         NULL,                     // Reserved, must be NULL
			                         &VerScore,                // Out: Matching score, see score enumeration FT_VER_SCORE
			                         &dFalseAcceptProbability, // Out: Probability to falsely match these fingerprints
			                         &bVerified);              // Returned result of fingerprint match. FT_TRUE means fingerprints matched.
			if (FT_OK <= rc) {
				if (FT_OK != rc) {
					TCHAR buffer[101] = {0};
					ULONG uSize = 100;
					_sntprintf_s(buffer, uSize, _TRUNCATE, _T("Warning: %ld (see dpRCodes.h)"), rc);
			        AddStatus(buffer);
				}

				if (bVerified == FT_TRUE) {
			        AddStatus(_T("Fingerprint Matches!"));
			        SetDlgItemText(IDC_EDIT_PROMPT, _T("Scan another finger to run verification again."));
					TCHAR buffer[101] = {0};
					ULONG uSize = 100;
					_sntprintf_s(buffer, uSize, _TRUNCATE, _T("%lf"), dFalseAcceptProbability);
					SetDlgItemText(IDE_PROBABILITY, buffer);
				}
				else {
			        AddStatus(_T("Fingerprint did not Match!"));
			        SetDlgItemText(IDC_EDIT_PROMPT, _T("Scan another finger to run verification again."));
					TCHAR buffer[101] = {0};
					ULONG uSize = 100;
					_sntprintf_s(buffer, uSize, _TRUNCATE, _T("%lf"), dFalseAcceptProbability);
					SetDlgItemText(IDE_PROBABILITY, buffer);
				}
			}
			else {
				TCHAR buffer[101] = {0};
				ULONG uSize = 100;
				_sntprintf_s(buffer, uSize, _TRUNCATE, _T("Verification Operation failed, Error: %ld."), rc);
				MessageBox(buffer, _T("Fingerprint Verification"), MB_OK | MB_ICONINFORMATION);
				SetDlgItemText(IDC_EDIT_PROMPT, _T("Scan your finger for verification again."));
			}
		}
	}
	catch(_com_error E) {
		hr = E.Error();
	}
	catch(...) {
		hr = E_UNEXPECTED;
	}
	delete [] pVerTemplate;

	if (FAILED(hr))
		ReportError(m_hWnd, hr);
}
