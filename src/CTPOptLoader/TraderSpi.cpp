#include <iostream>
#include <map>
#include <set>
#include <stdint.h>
#include <fstream>

#include "./ThostTraderApi/ThostFtdcTraderApi.h"
#include "../Share/StrUtil.hpp"
#include "../Includes/WTSTypes.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
namespace rj = rapidjson;

#include "TraderSpi.h"


USING_NS_OTP;

extern std::map<std::string, std::string>	MAP_NAME;
extern std::map<std::string, std::string>	MAP_SESSION;

#pragma warning(disable : 4996)

// USER_API����
extern CThostFtdcTraderApi* pUserApi;

// ���ò���
extern std::string	FRONT_ADDR;	// ǰ�õ�ַ
extern std::string	BROKER_ID;	// ���͹�˾����
extern std::string	INVESTOR_ID;// Ͷ���ߴ���
extern std::string	PASSWORD;	// �û�����
extern std::string	SAVEPATH;	//����λ��
extern std::string	HOTFILE;
extern std::string	APPID;
extern std::string	AUTHCODE;
extern uint32_t		CLASSMASK;

extern std::string COMM_FILE;		//�����Ʒ���ļ���
extern std::string CONT_FILE;		//����ĺ�Լ�ļ���

// ������
extern int iRequestID;

// �Ự����
TThostFtdcFrontIDType	FRONT_ID;	//ǰ�ñ��
TThostFtdcSessionIDType	SESSION_ID;	//�Ự���
TThostFtdcOrderRefType	ORDER_REF;	//��������

typedef struct _Commodity
{
	std::string	m_strName;
	std::string	m_strExchg;
	std::string	m_strProduct;
	std::string	m_strCurrency;
	std::string m_strSession;

	uint32_t	m_uVolScale;
	double		m_fPriceTick;
	uint32_t	m_uPrecision;

	ContractCategory	m_ccCategory;
	CoverMode			m_coverMode;
	PriceMode			m_priceMode;

} Commodity;
typedef std::map<std::string, Commodity> CommodityMap;
CommodityMap _commodities;

typedef struct _Contract
{
	std::string	m_strCode;
	std::string	m_strExchg;
	std::string	m_strName;
	std::string	m_strProduct;


	uint32_t	m_maxMktQty;
	uint32_t	m_maxLmtQty;

	OptionType	m_optType;
	std::string m_strUnderlying;
	double		m_strikePrice;
	double		m_dUnderlyingScale;
} Contract;
typedef std::map<std::string, Contract> ContractMap;
ContractMap _contracts;


std::string extractProductID(const char* instrument)
{
	std::string strRet;
	int nLen = 0;
	while('A' <= instrument[nLen] && instrument[nLen] <= 'z')
	{
		strRet += instrument[nLen];
		nLen++;
	}

	return strRet;
}

std::string extractProductName(const char* cname)
{
	std::string strRet;
	int idx = strlen(cname) - 1;
	while (isdigit(cname[idx]) && idx > 0)
	{
		idx--;
	}

	strRet.append(cname, idx + 1);
	return strRet;
}

std::set<std::string>	prod_set;

double convertInvalidDouble(double val)
{
	if(val == 5.5e-007)
		return -1;

	if(val == 0)
		return -1;

	return val;
}

void CTraderSpi::OnFrontConnected()
{
	std::cerr << "--->>> " << "OnFrontConnected" << std::endl;
	///�û���¼����
	ReqAuth();
}

void CTraderSpi::ReqAuth()
{
	CThostFtdcReqAuthenticateField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID.c_str());
	strcpy(req.UserID, INVESTOR_ID.c_str());
	//strcpy(req.UserProductInfo, m_strProdInfo.c_str());
	strcpy(req.AuthCode, AUTHCODE.c_str());
	strcpy(req.AppID, APPID.c_str());
	int iResult = pUserApi->ReqAuthenticate(&req, ++iRequestID);
	std::cerr << "--->>> �����ն���֤¼����: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << std::endl;
}

void CTraderSpi::OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	std::cerr << "--->>> " << "OnRspAuthenticate" << std::endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		ReqUserLogin();
	}
}

void CTraderSpi::ReqUserLogin()
{
	CThostFtdcReqUserLoginField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID.c_str());
	strcpy(req.UserID, INVESTOR_ID.c_str());
	strcpy(req.Password, PASSWORD.c_str());
	int iResult = pUserApi->ReqUserLogin(&req, ++iRequestID);
	std::cerr << "--->>> �����û���¼����: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << std::endl;
}

void CTraderSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
		CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	std::cerr << "--->>> " << "OnRspUserLogin" << std::endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		// ����Ự����
		FRONT_ID = pRspUserLogin->FrontID;
		SESSION_ID = pRspUserLogin->SessionID;
		int iNextOrderRef = atoi(pRspUserLogin->MaxOrderRef);
		iNextOrderRef++;
		sprintf(ORDER_REF, "%d", iNextOrderRef);
		///��ȡ��ǰ������
		m_lTradingDate = atoi(pUserApi->GetTradingDay());
		
		ReqQryInstrument();
	}
}

void CTraderSpi::ReqQryInstrument()
{
	CThostFtdcQryInstrumentField req;
	memset(&req, 0, sizeof(req));
	int iResult = pUserApi->ReqQryInstrument(&req, ++iRequestID);
	std::cerr << "--->>> �����ѯ��Լ: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << std::endl;
}

inline bool isOption(TThostFtdcProductClassType pClass)
{
	if (pClass == THOST_FTDC_PC_Options || pClass == THOST_FTDC_PC_SpotOption || pClass == THOST_FTDC_PC_SpotOption || pClass == THOST_FTDC_PC_ETFOption)
		return true;
	
	return false;
}

inline bool isFuture(TThostFtdcProductClassType pClass)
{
	return pClass == THOST_FTDC_PC_Futures;
}

inline bool isStock(TThostFtdcProductClassType pClass)
{
	return pClass == THOST_FTDC_PC_Stock;
}

inline ContractCategory wrapCategory(TThostFtdcProductClassType cType)
{
	switch (cType)
	{
	case THOST_FTDC_PC_Futures: return CC_Future;
	case THOST_FTDC_PC_Options: return CC_FutOption;
	case THOST_FTDC_PC_Combination: return CC_Combination;
	case THOST_FTDC_PC_Spot: return CC_Spot;
	case THOST_FTDC_PC_EFP: return CC_EFP;
	case THOST_FTDC_PC_SpotOption: return CC_SpotOption;
	case THOST_FTDC_PC_ETFOption: return CC_ETFOption;
	case THOST_FTDC_PC_Stock:return CC_Stock;
	default:
		throw std::runtime_error("non implemented category");
	}
}

inline const char* wrapProductID(const char* rawProductID)
{
	if (strcmp(rawProductID, "ASTOCK") == 0)
		return "STK";
	else if (strcmp(rawProductID, "ASTOCK_O") == 0)
		return "STKO";
	else if (strcmp(rawProductID, "ETF_O") == 0)
		return "ETFO";
	else
		return rawProductID;
}


void CTraderSpi::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo))
	{
		if (pInstrument)
		{		
			bool bOption = isOption(pInstrument->ProductClass);
			bool bFuture = isFuture(pInstrument->ProductClass);
			bool bStock = isStock(pInstrument->ProductClass);

			bool isGranted = false;
			if (bOption && (CLASSMASK & 2) != 0)
				isGranted = true;
			else if (bFuture && (CLASSMASK & 1) != 0)
				isGranted = true;
			else if (bStock && (CLASSMASK & 4) != 0)
				isGranted = true;

			if (isGranted)
			{

				std::cerr << "--->>> OnRspQryInstrument: " << pInstrument->ExchangeID << "." << pInstrument->InstrumentID << std::endl;
				std::string pid = wrapProductID(pInstrument->ProductID);
				std::string pname = MAP_NAME[pid];
				if(pname.empty())
				{
					std::stringstream ss;
					ss << pInstrument->ExchangeID << "." << pid;
					pname = MAP_NAME[ss.str()];
				}
				
				std::string cname = "";
				if (pname.empty())
				{
					cname = pInstrument->InstrumentName;
					pname = bFuture ? extractProductName(pInstrument->InstrumentName) : pInstrument->InstrumentName;
				}
				else
				{
					if (!bFuture)
					{
						cname = pInstrument->InstrumentName;
					}
					else
					{
						std::string month = pInstrument->InstrumentID;
						month = month.substr(pid.size());
						cname = pname + month;
					}
				}

				Contract contract;
				contract.m_strCode = pInstrument->InstrumentID;
				contract.m_strExchg = pInstrument->ExchangeID;
				contract.m_strName = StrUtil::trim(cname.c_str());
				contract.m_strProduct = pid;
				contract.m_maxMktQty = pInstrument->MaxMarketOrderVolume;
				contract.m_maxLmtQty = pInstrument->MaxLimitOrderVolume;

				contract.m_optType = bOption ? (OptionType)pInstrument->OptionsType : OT_None;
				contract.m_strUnderlying = pInstrument->UnderlyingInstrID;
				contract.m_strikePrice = pInstrument->StrikePrice;
				contract.m_dUnderlyingScale = pInstrument->UnderlyingMultiple;

				std::string key = StrUtil::printf("%s.%s", pInstrument->ExchangeID, pid.c_str());
				auto it = _commodities.find(key);
				if (it == _commodities.end())
				{
					Commodity commInfo;
					commInfo.m_strProduct = pid;
					commInfo.m_strName = StrUtil::trim(pname.c_str());
					commInfo.m_strExchg = pInstrument->ExchangeID;
					commInfo.m_strCurrency = "CNY";

					commInfo.m_strSession = MAP_SESSION[key];
					commInfo.m_ccCategory = wrapCategory(pInstrument->ProductClass);

					commInfo.m_uVolScale = (pInstrument->VolumeMultiple == 0 ? 1 : pInstrument->VolumeMultiple);
					commInfo.m_fPriceTick = pInstrument->PriceTick;

					CoverMode cm = CM_OpenCover;
					if (bFuture)
					{
						if (strcmp(pInstrument->ExchangeID, "SHFE") == 0 || strcmp(pInstrument->ExchangeID, "INE") == 0)
							cm = CM_CoverToday;
						//�������ľ���ƽ�񣬷��������ľ��ǿ�ƽ
					}

					commInfo.m_coverMode = cm;

					PriceMode pm = PM_Both;
					if (bFuture)
					{
						if (strcmp(pInstrument->ExchangeID, "SHFE") == 0 || strcmp(pInstrument->ExchangeID, "INE") == 0)
							pm = PM_Limit;
					}
					commInfo.m_priceMode = pm;

					if (pInstrument->PriceTick < 0.001)
						commInfo.m_uPrecision = 4;
					else if (pInstrument->PriceTick < 0.01)
						commInfo.m_uPrecision = 3;
					else if (pInstrument->PriceTick < 0.1)
						commInfo.m_uPrecision = 2;
					else if (pInstrument->PriceTick < 1)
						commInfo.m_uPrecision = 1;
					else
						commInfo.m_uPrecision = 0;

					_commodities[key] = commInfo;
				}

				key = StrUtil::printf("%s.%s", pInstrument->ExchangeID, pInstrument->InstrumentID);
				_contracts[key] = contract;
			}
		}
	}

	if(bIsLast)
	{
		DumpToJson();
		exit(0);
	}
}

void CTraderSpi::DumpToJson()
{
	//�����ļ���һ��contracts.json,һ��commodities.json
	//Json::Value jComms(Json::objectValue);
	rj::Document jComms(rj::kObjectType);
	{
		rj::Document::AllocatorType &allocator = jComms.GetAllocator();
		for (auto it = _commodities.begin(); it != _commodities.end(); it++)
		{
			const Commodity& commInfo = it->second;
			if (!jComms.HasMember(commInfo.m_strExchg.c_str()))
			{
				//jComms[commInfo.m_strExchg] = Json::Value(Json::objectValue);
				jComms.AddMember(rj::Value(commInfo.m_strExchg.c_str(), allocator), rj::Value(rj::kObjectType), allocator);
			}

			rj::Value jComm(rj::kObjectType);
			jComm.AddMember("covermode", (uint32_t)commInfo.m_coverMode, allocator);
			jComm.AddMember("pricemode", (uint32_t)commInfo.m_priceMode, allocator);
			jComm.AddMember("category", (uint32_t)commInfo.m_ccCategory, allocator);
			jComm.AddMember("precision", commInfo.m_uPrecision, allocator);
			jComm.AddMember("pricetick", commInfo.m_fPriceTick, allocator);
			jComm.AddMember("volscale", commInfo.m_uVolScale, allocator);

			jComm.AddMember("name", rj::Value(commInfo.m_strName.c_str(), allocator), allocator);
			jComm.AddMember("exchg", rj::Value(commInfo.m_strExchg.c_str(), allocator), allocator);
			jComm.AddMember("session", rj::Value(commInfo.m_strSession.c_str(), allocator), allocator);
			jComm.AddMember("holiday", rj::Value("CHINA", allocator), allocator);

			//jComms[commInfo.m_strExchg][commInfo.m_strProduct] = jComm;
			jComms[commInfo.m_strExchg.c_str()].AddMember(rj::Value(commInfo.m_strProduct.c_str(), allocator), jComm, allocator);
		}
	}

	//Json::Value jContracts(Json::objectValue);
	rj::Document jContracts(rj::kObjectType);
	{
		rj::Document::AllocatorType &allocator = jContracts.GetAllocator();
		for (auto it = _contracts.begin(); it != _contracts.end(); it++)
		{
			const Contract& cInfo = it->second;
			if (!jContracts.HasMember(cInfo.m_strExchg.c_str()))
			{
				//jComms[commInfo.m_strExchg] = Json::Value(Json::objectValue);
				jContracts.AddMember(rj::Value(cInfo.m_strExchg.c_str(), allocator), rj::Value(rj::kObjectType), allocator);
			}

			rj::Value jcInfo(rj::kObjectType);

			jcInfo.AddMember("name", rj::Value(cInfo.m_strName.c_str(), allocator), allocator);
			jcInfo.AddMember("code", rj::Value(cInfo.m_strCode.c_str(), allocator), allocator);
			jcInfo.AddMember("exchg", rj::Value(cInfo.m_strExchg.c_str(), allocator), allocator);
			jcInfo.AddMember("product", rj::Value(cInfo.m_strProduct.c_str(), allocator), allocator);

			jcInfo.AddMember("maxlimitqty", cInfo.m_maxLmtQty, allocator);
			jcInfo.AddMember("maxmarketqty", cInfo.m_maxMktQty, allocator);

			if(cInfo.m_optType != OT_None)
			{
				//OptionType	m_optType;
				//std::string m_strUnderlying;
				//double		m_strikePrice;
				//double		m_dUnderlyingScale;
				rj::Value optInfo(rj::kObjectType);
				optInfo.AddMember("optiontype", (uint32_t)cInfo.m_optType, allocator);
				optInfo.AddMember("underlying", rj::Value(cInfo.m_strUnderlying.c_str(), allocator), allocator);
				optInfo.AddMember("strikeprice", cInfo.m_strikePrice, allocator);
				optInfo.AddMember("underlyingscale", cInfo.m_dUnderlyingScale, allocator);

				jcInfo.AddMember("option", optInfo, allocator);
			}

			jContracts[cInfo.m_strExchg.c_str()].AddMember(rj::Value(cInfo.m_strCode.c_str(), allocator), jcInfo, allocator);
		}
	}

	std::ofstream ofs;
	std::string path = SAVEPATH;
	path += COMM_FILE;
	ofs.open(path);
	{
		rj::StringBuffer sb;
		rj::PrettyWriter<rj::StringBuffer> writer(sb);
		jComms.Accept(writer);
		ofs << sb.GetString();
	}
	ofs.close();

	path = SAVEPATH;
	path += CONT_FILE;
	ofs.open(path);
	{
		rj::StringBuffer sb;
		rj::PrettyWriter<rj::StringBuffer> writer(sb);
		jContracts.Accept(writer);
		ofs << sb.GetString();
	}
	ofs.close();
}


void CTraderSpi:: OnFrontDisconnected(int nReason)
{
	std::cerr << "--->>> " << "OnFrontDisconnected" << std::endl;
	std::cerr << "--->>> Reason = " << nReason << std::endl;
}
		

void CTraderSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	std::cerr << "--->>> " << "OnRspError" << std::endl;
	IsErrorRspInfo(pRspInfo);
}

bool CTraderSpi::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	// ���ErrorID != 0, ˵���յ��˴������Ӧ
	bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	if (bResult)
		std::cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg << std::endl;
	return bResult;
}