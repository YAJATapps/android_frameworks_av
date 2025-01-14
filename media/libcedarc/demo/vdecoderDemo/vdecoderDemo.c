/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : vdecoderDemo.c
 * Description : vdecoderDemo
 * History :
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include "vdecoder.h"
#include "memoryAdapter.h"
#include "rawStreamParser.h"
#include "cdc_log.h"
#include "CdcIniparserapi.h"

#define DEBUG_TIME_INFO 0
#define DEBUG_COST_DRAM_ENABLE 0

#define DRAM_COST_MAX_THREAD_NUM 8
#define FRAME_COUNT 64
#define DISPLAY_PICTUER_LIST_NUM 5
#define DISPLAY_HOLDING_BUFFERS 2
#define DISPLAY_HOLDING_NUM 0
#define DISPLAY_COST_TIME 2

#define DEMO_PARSER_MAX_STREAM_NUM 1024
#define DEMO_FILE_NAME_LEN (2048)

#define DEMO_PARSER_ERROR (1 << 0)
#define DEMO_DECODER_ERROR (1 << 1)
#define DEMO_DISPLAY_ERROR (1 << 2)
#define DEMO_DECODE_FINISH (1 << 3)
#define DEMO_PARSER_EXIT   (1 << 5)
#define DEMO_DECODER_EXIT  (1 << 6)
#define DEMO_DISPLAY_EXIT  (1 << 7)
#define DEMO_ERROR    (DEMO_PARSER_ERROR | DEMO_DECODER_ERROR | DEMO_DISPLAY_ERROR)
#define DEMO_EXIT    (DEMO_ERROR | DEMO_DECODE_FINISH)

#define DECODER_MAX_NUM (5)

typedef struct MultiThreadCtx
{
    pthread_rwlock_t rwrock;
    int nEndofStream;
    int state;
}MultiThreadCtx;

typedef struct {
    char *pInputFile[DECODER_MAX_NUM];
    FILE *pInputFileFp[DECODER_MAX_NUM];
    char *pOutputFile[DECODER_MAX_NUM];
    int  nFinishNum[DECODER_MAX_NUM];
    int  nSavePictureStartNumber[DECODER_MAX_NUM];
    int  nDramCostThreadNum[DECODER_MAX_NUM];
    char *pSaveShaFile[DECODER_MAX_NUM];
    int nVeFreq[DECODER_MAX_NUM];
    char *pCompareShaFile[DECODER_MAX_NUM];
    int  nCodecFormat[DECODER_MAX_NUM];
    int nOutputPixelFormat[DECODER_MAX_NUM];
    int nSavePictureNumber[DECODER_MAX_NUM];
    int nDecoderNum;
}Dec_Params;


typedef struct DecDemo
{
    VideoDecoder *pVideoDec;
    MultiThreadCtx thread;
    long long totalTime;
    long long DurationTime;
    int  nDispFrameCount;
    int  nFinishNum;
    int  nDramCostThreadNum;
    int  nDecodeFrameCount;
    int  nCodecFormat;
    char *pInputFile;
    FILE *pInputFileFp;
    char *pOutputFile;
    char *pSaveShaFile;
    FILE *pSaveShaFp;
    char *pCompareShaFile;
    FILE *pCompareShaFp;
    int   nCompareShaErrorCount;
    pthread_mutex_t parserMutex;
    /* start to save yuv picture,
     * when decoded picture order >=  nSavePictureStartNumber*/
    int  nSavePictureStartNumber;
    /* the saved picture number */
    int  nSavePictureNumber;
    struct ScMemOpsS* memops;
    int nVeFreq;
    RawParserContext* pParser;
    int nOutputPixelFormat;
    int nChannel;
    Dec_Params pParams;
}DecDemo;

typedef struct display
{
    VideoPicture* picture;
    int flag;
    struct display *next;
}display;

typedef enum {
    INPUT,
    HELP,
    DECODE_FRAME_NUM,
    SAVE_FRAME_START,
    SAVE_FRAME_NUM,
    SAVE_FRAME_FILE,
    COST_DRAM_THREAD_NUM,
    SAVE_SHA_FILE,
    SETUP_VE_FREQ,
    COMPARE_SHA,
    CODEC_FORMAT,
    DECODER_NUM,
    OUTPUT_FORMAT,
    INVALID
}ARGUMENT_T;


typedef struct {
    char Short[16];
    char Name[128];
    ARGUMENT_T argument;
    char Description[512];
}argument_t;



static const argument_t ArgumentMapping[] =
{
    { "-h",  "--help",    HELP,
        "Print this help" },
    { "-i",  "--input",   INPUT,
        "Input file" },
    { "-n",  "--decode_frame_num",   DECODE_FRAME_NUM,
        "After display n frames, decoder stop" },
    { "-ss",  "--save_frame_start",  SAVE_FRAME_START,
        "After display ss frames, saving pictures begin" },
    { "-sn",  "--save_frame_num",  SAVE_FRAME_NUM,
        "After sn frames saved, stop saving picture" },
    { "-o",  "--save_frame_file",  SAVE_FRAME_FILE,
        "saving picture file" },
    { "-cn",  "--cost_dram_thread_num",  COST_DRAM_THREAD_NUM,
        "create cn threads to cost dram, or disturb cpu, test decoder robust" },
    { "-sha", "--save_sha",SAVE_SHA_FILE,
        "save sha file"},
    { "-vefreq", "--setup_ve_freq",SETUP_VE_FREQ,
        "setup ve freq"},
    { "-cmpsha", "--compare_sha",COMPARE_SHA,
        "compare sha value"},
    { "-codFmat", "--nCodecFormat",CODEC_FORMAT,
        "input file codecformat"},
    { "-decNum", "--decoder_num",DECODER_NUM,
        "decoder number"},
    { "-outFmat", "--output_format",OUTPUT_FORMAT,
        "output file PixelFormat"},
};

static void PrintDemoUsage(void)
{
    int i = 0;
    int num = sizeof(ArgumentMapping) / sizeof(argument_t);
    logd("Usage:");
    while(i < num)
    {
        logd("%s %-32s  %s", ArgumentMapping[i].Short, ArgumentMapping[i].Name,
                ArgumentMapping[i].Description);
        i++;
    }
}

ARGUMENT_T GetArgument(char *name)
{
    int i = 0;
    int num = sizeof(ArgumentMapping) / sizeof(argument_t);
    while(i < num)
    {
        if((0 == strcmp(ArgumentMapping[i].Name, name)) ||
            ((0 == strcmp(ArgumentMapping[i].Short, name)) &&
             (0 != strcmp(ArgumentMapping[i].Short, "--"))))
        {
            return ArgumentMapping[i].argument;
        }
        i++;
    }
    return INVALID;
}

void  ParseArgument(Dec_Params *params, char** argv, int pos ,int length)
{
    ARGUMENT_T arg;
//    int len = strlen(value);
    int len = 0;
    int i = 0;
    if(len > DEMO_FILE_NAME_LEN)
        return;
    arg = GetArgument(argv[pos]);
    switch(arg)
    {
        case HELP:
            PrintDemoUsage();
            exit(-1);
        case INPUT:
            //sprintf(Decoder->pInputFile, "file://");
            for(i = 0; i <length -1 ; i++)
            {
                snprintf(params->pInputFile[i], DEMO_FILE_NAME_LEN, "%2047s", argv[pos+i+1]);
                logd(" get input file[%d]: %s ", i, params->pInputFile[i]);
            }
            break;
        case DECODE_FRAME_NUM:
            for(i = 0; i <length -1 ; i++)
            {
                sscanf(argv[pos+i+1], "%32d", &params->nFinishNum[i]);
                logd(" decode frame num [%d]: %d", i,params->nFinishNum[i]);
            }
            break;
        case SAVE_FRAME_START:
            for(i = 0; i <length -1 ; i++)
            {
                sscanf(argv[pos+i+1], "%32d", &params->nSavePictureStartNumber[i]);
                loge(" save frame start  [%d]: %d", i,params->nSavePictureStartNumber[i]);
            }
            break;
        case SAVE_FRAME_NUM:
            for(i = 0; i <length -1 ; i++)
            {
                sscanf(argv[pos+i+1], "%32d", &params->nSavePictureNumber[i]);
                logd(" save frame num  [%d]: %d", i,params->nSavePictureNumber[i]);
            }
            break;
        case COST_DRAM_THREAD_NUM:
            for(i = 0; i <length -1 ; i++)
            {
                sscanf(argv[pos+i+1], "%32d", &params->nDramCostThreadNum[i]);
                logd(" cost  dram thread num  [%d]: %d", i,params->nDramCostThreadNum[i]);
            }
            break;
        case SAVE_FRAME_FILE:
            for(i = 0; i <length -1 ; i++)
            {
                snprintf(params->pOutputFile[i], DEMO_FILE_NAME_LEN, "%2047s", argv[pos+i+1]);
                logd(" out put file: [%d] %s ", i,params->pOutputFile[i]);
            }
            break;
        case SAVE_SHA_FILE:
            for(i = 0; i <length -1 ; i++)
            {
                snprintf(params->pSaveShaFile[i], DEMO_FILE_NAME_LEN, "%2047s", argv[pos+i+1]);
                logd(" save sha file: %s ", params->pSaveShaFile[i]);
            }
            break;
        case SETUP_VE_FREQ:
            for(i = 0; i <length -1 ; i++)
            {
                sscanf(argv[pos+i+1], "%32d", &params->nVeFreq[i]);
                logd(" setup ve freq: %d ", params->nVeFreq[i]);
            }
            break;
        case COMPARE_SHA:
            for(i = 0; i <length -1 ; i++)
            {
                snprintf(params->pCompareShaFile[i], DEMO_FILE_NAME_LEN, "%2047s", argv[pos+i+1]);
                logd(" get compare sha file: %s ", params->pCompareShaFile[i]);

                sprintf(params->pSaveShaFile[i],"%s_cur",params->pCompareShaFile[i]);
                logd(" get current sha file: %s ", params->pSaveShaFile[i]);
            }
            break;
        case CODEC_FORMAT:
            for(i = 0; i <length -1 ; i++)
            {
                sscanf(argv[pos+i+1], "%32d", &params->nCodecFormat[i]);
                logd("log nCodec Format %d",params->nCodecFormat[i]);
            }
            break;
        case DECODER_NUM:
            for(i = 0; i <length -1 ; i++)
            {
                sscanf(argv[pos+i+1], "%d", &params->nDecoderNum);
                logd("log decoder num [%d]:%d",i,params->nDecoderNum);
            }
            break;
        case OUTPUT_FORMAT:
            for(i = 0; i <length -1 ; i++)
            {
                sscanf(argv[pos+i+1], "%32d", &params->nOutputPixelFormat[i]);
                logd("log nOutputPixelFormat [%d]:%d",i,params->nOutputPixelFormat[i]);
            }
            break;
        case INVALID:
        default:
            logd("unknowed argument :  %s", argv[pos]);
            break;
    }
}

static long long GetNowUs(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000ll + tv.tv_usec;
}

static int initDecoder(DecDemo *Decoder)
{
    AddVDPlugin();

    int nRet;
    VConfig             VideoConf;
    VideoStreamInfo     VideoInfo;
    //struct CdxProgramS *program;

    memset(&VideoConf, 0, sizeof(VConfig));
    memset(&VideoInfo, 0, sizeof(VideoStreamInfo));
    Decoder->memops = MemAdapterGetOpsS();
    if(Decoder->memops == NULL)
    {
        loge("memops is NULL");
        return -1;
    }
    CdcMemOpen(Decoder->memops);
    pthread_mutex_init(&Decoder->parserMutex, NULL);

    #if 0
    nRet = CdxParserPrepare(&Decoder->source, 0, &Decoder->parserMutex,
            &bForceExit, &Decoder->parser, &stream, NULL, NULL);
    if(nRet < 0 || Decoder->parser == NULL)
    {
        loge(" decoder open parser error nRet = %d, Decoder->parser: %p", nRet, Decoder->parser);
        return -1;
    }
    logv(" before CdxParserGetMediaInfo() ");
    nRet = CdxParserGetMediaInfo(Decoder->parser, &Decoder->mediaInfo);
    if(nRet != 0)
    {
        loge(" decoder parser get media info error ");
        return -1;
    }
    #endif
    logv(" before CreateVideoDecoder() ");
    Decoder->pVideoDec = CreateVideoDecoder();
    if(Decoder->pVideoDec == NULL)
    {
        loge(" decoder demom CreateVideoDecoder() error ");
        return -1;
    }
    logv(" before InitializeVideoDecoder() ");
    #if 0
    program = &(Decoder->mediaInfo.program[Decoder->mediaInfo.programIndex]);
    for (i = 0; i < 1; i++)
    {
        VideoStreamInfo *vp = &VideoInfo;
        vp->eCodecFormat = program->video[i].eCodecFormat;
        vp->nWidth = program->video[i].nWidth;
        vp->nHeight = program->video[i].nHeight;
        vp->nFrameRate = program->video[i].nFrameRate;
        vp->nFrameDuration = program->video[i].nFrameDuration;
        vp->nAspectRatio = program->video[i].nAspectRatio;
        vp->bIs3DStream = program->video[i].bIs3DStream;
        vp->nCodecSpecificDataLen = program->video[i].nCodecSpecificDataLen;
        vp->pCodecSpecificData = program->video[i].pCodecSpecificData;
    }
    #endif

    //int nCodecFormat = RawParserProbe(Decoder->pParser);
    int nCodecFormat =  Decoder->nCodecFormat;
    if(nCodecFormat == VIDEO_FORMAT_H264)
    {
        VideoInfo.eCodecFormat = VIDEO_CODEC_FORMAT_H264;
        Decoder->pParser->streamType = VIDEO_FORMAT_H264;
    }
    else if(nCodecFormat == VIDEO_FORMAT_H265)
    {
        VideoInfo.eCodecFormat = VIDEO_CODEC_FORMAT_H265;
        Decoder->pParser->streamType = VIDEO_FORMAT_H265;
    }
    else if(nCodecFormat == VIDEO_FORMAT_AV1)
    {
        VideoInfo.eCodecFormat = VIDEO_CODEC_FORMAT_H264;
        //Decoder->pParser->streamType = VIDEO_FORMAT_AV1;
    }

    logd("nCodecFormat = %d, 0x%x",nCodecFormat, VideoInfo.eCodecFormat);

    VideoConf.eOutputPixelFormat  = Decoder->nOutputPixelFormat;
    logd("eOutputPixelFormat = %d Decoder->nChannel: %d", VideoConf.eOutputPixelFormat,Decoder->nChannel);

    VideoConf.nDeInterlaceHoldingFrameBufferNum = CdcGetConfigParamterInt("pic_4di_num", 2);
    VideoConf.nDisplayHoldingFrameBufferNum = CdcGetConfigParamterInt("pic_4list_num", 3);
    VideoConf.nRotateHoldingFrameBufferNum = CdcGetConfigParamterInt("pic_4rotate_num", 0);
    VideoConf.nDecodeSmoothFrameBufferNum = CdcGetConfigParamterInt("pic_4smooth_num", 3);
    VideoConf.memops = Decoder->memops;
    VideoConf.nVeFreq = Decoder->nVeFreq;

    logd("**** set the ve freq = %d",VideoConf.nVeFreq);

    nRet = InitializeVideoDecoder(Decoder->pVideoDec, &VideoInfo, &VideoConf);
    logv(" after InitializeVideoDecoder() ");
    if(nRet != 0)
    {
        loge("decoder demom initialize video decoder fail.");
        DestroyVideoDecoder(Decoder->pVideoDec);
        Decoder->pVideoDec = NULL;
    }

    pthread_rwlock_init(&Decoder->thread.rwrock, NULL);

    logd(" initDecoder OK ");
    return 0;
}

static void addPictureToList(VideoDecoder *pVideoDec, display *pDisList,
        display **h, display **r, VideoPicture* pPicture)
{
    int i;
    display *node = NULL;
    display *DisHeader = *h;
    display *DisRear = *r;
    if(pPicture == NULL || pDisList == NULL)
    {
        loge(" add picuter to list error");
        return;
    }
    for(i = 0; i < DISPLAY_PICTUER_LIST_NUM; i++)
    {
        if(pDisList[i].flag == 0)
        {
            node = &pDisList[i];
            node->flag = 1;
            node->picture = pPicture;
            break;
        }
    }
    if(DisHeader == NULL && DisRear == NULL)
    {
        DisHeader = DisRear = node;
    }
    else
    {
        DisRear->next = node;
        node->next = NULL;
        DisRear = node;
    }

    i = 1;
    node = DisHeader;
    while(node != NULL && node->next != NULL)
    {
        i += 1;
        node = node->next;
    }
    if(i >= DISPLAY_HOLDING_BUFFERS)
    {
        node = DisHeader;
        DisHeader = DisHeader->next;
        node->next = NULL;
        node->flag = 0;
        usleep(DISPLAY_COST_TIME * 1000); /* displaying one picture use some time */
        logv(" display one picture. pts: %lld ", node->picture->nPts);
        ReturnPicture(pVideoDec, node->picture);
    }
    *h = DisHeader;
    *r = DisRear;
}

static int ReturnAllPicture(VideoDecoder *pVideoDec, display *DisHeader)
{
    int num = 0;
    while(DisHeader)
    {
        ReturnPicture(pVideoDec, DisHeader->picture);
        DisHeader->flag = 0;
        DisHeader = DisHeader->next;
        num += 1;
    }
    return num;
}

void* DecodeThread(void* param)
{
    DecDemo  *pVideoDecoder;
    VideoDecoder *pVideoDec;
    int nRet=0, nStreamNum=0, i=0, state=0;
    int nEndOfStream=0;
 
    pVideoDecoder = (DecDemo *)param;
    nEndOfStream = 0;

    pVideoDec = pVideoDecoder->pVideoDec;
    logv(" DecodeThread(), thread created ");

    i = 0;
    nStreamNum = VideoStreamFrameNum(pVideoDec, 0);
    while(nStreamNum < 200)
    {
        usleep(2*1000);
        i++;
        if(i > 100)
            break;
        nStreamNum = VideoStreamFrameNum(pVideoDec, 0);
    }
    logv(" data trunk number: %d, i = %d ", nStreamNum, i);

    while(1)
    {
        /* step 1: get stream data */
        usleep(50);
        pthread_rwlock_wrlock(&pVideoDecoder->thread.rwrock);
        nEndOfStream = pVideoDecoder->thread.nEndofStream;
        state = pVideoDecoder->thread.state;
        pthread_rwlock_unlock(&pVideoDecoder->thread.rwrock);
        if(state & DEMO_EXIT)
        {
            if(state & DEMO_ERROR)
            {
                loge(" decoer thread recieve an error singnal,  exit..... ");
            }
            if(state & DEMO_DECODE_FINISH)
            {
                logd(" decoer thread recieve a finish singnal,  exit.....  ");
            }
            break;
        }

        logv(" DecodeThread(), DecodeVideoStream() start .... ");
        nRet = DecodeVideoStream(pVideoDec, nEndOfStream /*eos*/,
                0/*key frame only*/, 0/*drop b frame*/,
                0/*current time*/);
//        logd(" ------- decoderThread. one frame cost time: %lld ", deltaTime);

        if(nEndOfStream == 1 && nRet == VDECODE_RESULT_NO_BITSTREAM)
        {
            logd(" decoer thread finish decoding.  exit..... ");
            break;
        }
        if(nRet == VDECODE_RESULT_KEYFRAME_DECODED ||
                nRet == VDECODE_RESULT_FRAME_DECODED)
            pVideoDecoder->nDecodeFrameCount++;

        if(nRet < 0)
        {
            loge(" decoder return error. decoder exit ");
            pthread_rwlock_wrlock(&pVideoDecoder->thread.rwrock);
            pVideoDecoder->thread.state |= DEMO_DECODER_ERROR;
            pthread_rwlock_unlock(&pVideoDecoder->thread.rwrock);
            break;
        }
    }

    pthread_rwlock_wrlock(&pVideoDecoder->thread.rwrock);
    pVideoDecoder->thread.state |= DEMO_DECODER_EXIT;
    pthread_rwlock_unlock(&pVideoDecoder->thread.rwrock);

    logd(" decoder thread exit.... ");
    pthread_exit(NULL);
    return 0;
}

static void savePicture(VideoPicture* pPicture, DecDemo  *pVideoDecoder)
{
    char *file = pVideoDecoder->pOutputFile;

    int nSizeY, nSizeUV, nSize;
    FILE *fp = NULL;
    char *pData = NULL;

    if(pPicture == NULL)
    {
        loge(" demo decoder save picture error, picture pointer equals NULL ");
        return;
    }
    fp = fopen(file, "wb");
    if(fp == NULL)
    {
        loge(" demo decoder save picture error, open file fail ");
        return;
    }
    nSizeY = pPicture->nWidth * pPicture->nHeight;
    nSizeUV = nSizeY >> 2;
    logd(" save picture to file: %s, size: %dx%d, top: %d, bottom: %d, left: %d, right: %d",
            file, pPicture->nWidth, pPicture->nHeight,pPicture->nTopOffset,
            pPicture->nBottomOffset, pPicture->nLeftOffset, pPicture->nRightOffset);
    int nSaveLen;
    if(pPicture->bEnableAfbcFlag)
         nSaveLen = pPicture->nAfbcSize;
    else
         nSaveLen = pPicture->nWidth * pPicture->nHeight * 3/2;

    CdcMemFlushCache(pVideoDecoder->memops,pPicture->pData0, nSaveLen);

    /* save y */
    pData = pPicture->pData0;
    nSize = nSizeY;
    fwrite(pData, 1, nSize, fp);
    if(pVideoDecoder->nOutputPixelFormat == PIXEL_FORMAT_YUV_PLANER_420 ||
        pVideoDecoder->nOutputPixelFormat == PIXEL_FORMAT_YV12)
    {
        /* save u */
        pData = pPicture->pData0 + nSizeY + nSizeUV;
        nSize = nSizeUV;
        fwrite(pData, 1, nSize, fp);

        /* save v */
        pData = pPicture->pData0 + nSizeY;
        nSize = nSizeUV;
        fwrite(pData, 1, nSize, fp);
    }
    else if(pVideoDecoder->nOutputPixelFormat == PIXEL_FORMAT_NV21 ||
        pVideoDecoder->nOutputPixelFormat == PIXEL_FORMAT_NV12)
    {
        /* save uv */
        pData = pPicture->pData0 + nSizeY;
        nSize = nSizeUV*2;
        fwrite(pData, 1, nSize, fp);
    }
    fclose(fp);
}

void *displayPictureThreadFunc(void* param)
{
    DecDemo  *pVideoDecoder;
    VideoDecoder *pVideoDec;
    VideoPicture* pPicture;
    display *DisHeader, *DisRear, *pDisList;
    long long nTime, DurationTime;
    int state, nDispFrameCount, nValidPicNum;

    pVideoDecoder = (DecDemo *)param;

    DisHeader = NULL;
    DisRear = NULL;
    nDispFrameCount = 0;
    nValidPicNum = 0;
    DurationTime = 0;
    pVideoDec = pVideoDecoder->pVideoDec;

    pDisList = calloc(DISPLAY_PICTUER_LIST_NUM, sizeof(display));
    if(pDisList == NULL)
    {
        pthread_rwlock_wrlock(&pVideoDecoder->thread.rwrock);
        pVideoDecoder->thread.state |= DEMO_DISPLAY_ERROR;
        pthread_rwlock_unlock(&pVideoDecoder->thread.rwrock);
        logd(" display thread calloc memory fial ");
        goto display_thread_exit;
    }
    logd(" display thread starting..... ");
    pVideoDecoder->nDispFrameCount = 0;
    nTime = GetNowUs();
    while(1)
    {
        usleep(100);
        pthread_rwlock_wrlock(&pVideoDecoder->thread.rwrock);
        state = pVideoDecoder->thread.state;
        pthread_rwlock_unlock(&pVideoDecoder->thread.rwrock);
        if(state & DEMO_EXIT)
        {
            loge(" display thread recieve an error singnal,  exit..... ");
            break;
        }
        pPicture = RequestPicture(pVideoDec, 0/*the major stream*/);
        if(pPicture != NULL)
        {
            logv(" decoder get one picture, size: %dx%d ", pPicture->nWidth, pPicture->nHeight);
            logd(" picture size: %dx%d ", pPicture->nWidth, pPicture->nHeight);
            nTime = GetNowUs() - nTime;
            DurationTime += nTime;
            nDispFrameCount += 1;

            if(nDispFrameCount >= pVideoDecoder->nSavePictureStartNumber &&
                    nDispFrameCount < (pVideoDecoder->nSavePictureStartNumber +
                                    pVideoDecoder->nSavePictureNumber))
            {
                logd(" saving picture number: %d ", nDispFrameCount);
                savePicture(pPicture, pVideoDecoder);
            }

            if(nDispFrameCount >= pVideoDecoder->nFinishNum)
            {
                loge(" display thread get enungh frame, exit ...");
                pthread_rwlock_wrlock(&pVideoDecoder->thread.rwrock);
                pVideoDecoder->thread.state |= DEMO_DECODE_FINISH;
                pthread_rwlock_unlock(&pVideoDecoder->thread.rwrock);
                break;
            }
#if DEBUG_TIME_INFO
            if(nDispFrameCount >= FRAME_COUNT)
            {
                float fps, avg;
                pVideoDecoder->nDispFrameCount += nDispFrameCount;
                pVideoDecoder->DurationTime += DurationTime;
                fps = (float)(DurationTime / 1000);
                fps = (nDispFrameCount * 1000) / fps;
                avg = (float)(pVideoDecoder->DurationTime / 1000);
                avg = (pVideoDecoder->nDispFrameCount * 1000) / avg;

                loge(" decoder speed info. current speed: %.2f, average speed: %.2f ", fps, avg);
                DurationTime = 0;
                nDispFrameCount = 0;
            }
#endif
            nTime = GetNowUs();
            addPictureToList(pVideoDec, pDisList, &DisHeader, &DisRear, pPicture);
        }
        else
        {
            pthread_rwlock_wrlock(&pVideoDecoder->thread.rwrock);
            state = pVideoDecoder->thread.state;
            pthread_rwlock_unlock(&pVideoDecoder->thread.rwrock);
            if(state & DEMO_DECODER_EXIT)
            {
                nValidPicNum = ValidPictureNum(pVideoDec, 0);
                logv(" display thread find that decode thread had exit ");
                if(nValidPicNum <= 0)
                    break;
            }
        }
    }

    pVideoDecoder->nDispFrameCount += nDispFrameCount;
    pthread_rwlock_wrlock(&pVideoDecoder->thread.rwrock);
    pVideoDecoder->thread.state |= DEMO_DISPLAY_EXIT;
    pthread_rwlock_unlock(&pVideoDecoder->thread.rwrock);
    ReturnAllPicture(pVideoDec, DisHeader);
    logd(" display thread exit....disp frame num: %d ", pVideoDecoder->nDispFrameCount);
display_thread_exit:
    if(pDisList)
        free(pDisList);
    sync();
    pthread_exit(NULL);
    return 0;
}

void *parserThreadFunc(void* param)
{
    DecDemo *pDec;
    RawParserContext *parser;
    VideoDecoder *pVideoDec;
    int nRet, nStreamNum, state;
    int nValidSize;
    int nRequestDataSize, trytime;
    unsigned char *buf;
    VideoStreamDataInfo  dataInfo;
    DataPacketT packet;

    buf = malloc(1024*1024);
    if(buf == NULL)
    {
        loge(" parser thread malloc error ");
        goto parser_exit;
    }
    pDec = (DecDemo *)param;
    pVideoDec = pDec->pVideoDec;
    parser = pDec->pParser;
    memset(&packet, 0, sizeof(packet));
    logv(" parserThreadFunc(), thread created ! ");
    state = 0;
    trytime = 0;
    while (0 == RawParserPrefetch(parser, &packet))
    {
        usleep(50);
        nValidSize = VideoStreamBufferSize(pVideoDec, 0) - VideoStreamDataSize(pVideoDec, 0);
        nRequestDataSize = packet.length;

        pthread_rwlock_wrlock(&pDec->thread.rwrock);
        state = pDec->thread.state;
        pthread_rwlock_unlock(&pDec->thread.rwrock);
        if(state & DEMO_EXIT)
        {
            loge(" hevc parser receive other thread error. exit flag ");
            goto parser_exit;
        }
        if(trytime >= 2000)
        {
            loge("  parser thread trytime >= 2000, maybe some error happen ");
            pthread_rwlock_wrlock(&pDec->thread.rwrock);
            pDec->thread.state |= DEMO_PARSER_ERROR;
            pthread_rwlock_unlock(&pDec->thread.rwrock);
            goto parser_exit;
        }

        if(nRequestDataSize > nValidSize)
        {
            usleep(50 * 1000);
            trytime++;
            continue;
        }

        nRet = RequestVideoStreamBuffer(pVideoDec,
                                        nRequestDataSize,
                                        (char**)&packet.buf,
                                        &packet.buflen,
                                        (char**)&packet.ringBuf,
                                        &packet.ringBufLen,
                                        0);
        if(nRet != 0)
        {
            logw(" RequestVideoStreamBuffer fail. request size: %d, valid size: %d ",
                    nRequestDataSize, nValidSize);
            usleep(50*1000);
            continue;
        }
        if(packet.buflen + packet.ringBufLen < nRequestDataSize)
        {
            loge(" RequestVideoStreamBuffer fail, require size is too small ");
            pthread_rwlock_wrlock(&pDec->thread.rwrock);
            pDec->thread.state |= DEMO_PARSER_ERROR;
            pthread_rwlock_unlock(&pDec->thread.rwrock);
            goto parser_exit;
        }

        trytime = 0;
        nStreamNum = VideoStreamFrameNum(pVideoDec, 0);
        if(nStreamNum > DEMO_PARSER_MAX_STREAM_NUM)
        {
            usleep(50*1000);
        }
        nRet = RawParserRead(parser, &packet);
        if(nRet != 0)
        {
            loge(" parser thread read video data error ");
            pthread_rwlock_wrlock(&pDec->thread.rwrock);
            pDec->thread.state |= DEMO_PARSER_ERROR;
            pthread_rwlock_unlock(&pDec->thread.rwrock);
            goto parser_exit;
        }
        memset(&dataInfo, 0, sizeof(VideoStreamDataInfo));
        dataInfo.pData          = packet.buf;
        dataInfo.nLength      = packet.length;
        dataInfo.nPts          = packet.pts;
        dataInfo.nPcr          = packet.pcr;
        dataInfo.bIsFirstPart = 1;
        dataInfo.bIsLastPart = 1;
        dataInfo.bValid = 1;
        nRet = SubmitVideoStreamData(pVideoDec , &dataInfo, 0);
        if(nRet != 0)
        {
            loge(" parser thread  SubmitVideoStreamData() error ");
            pthread_rwlock_wrlock(&pDec->thread.rwrock);
            pDec->thread.state |= DEMO_PARSER_ERROR;
            pthread_rwlock_unlock(&pDec->thread.rwrock);
            goto parser_exit;
        }
    }

    pthread_rwlock_wrlock(&pDec->thread.rwrock);
    pDec->thread.nEndofStream = 1;
    pDec->thread.state |= DEMO_PARSER_EXIT;
    pthread_rwlock_unlock(&pDec->thread.rwrock);

parser_exit:
    if(buf)
        free(buf);
    logv(" parser exit..... ");
    pthread_exit(NULL);
    return 0;
}

/*
 * DRAMcostTHread() thread make cpu do some other thing, cost DRAM bandwidth
 * */
void *DRAMcostTHread(void *arg)
{
#define MEMORY_STRIDE 1920
#define MEMORY_NUM 8
#define MEMORY_BLOCK (1080*MEMORY_STRIDE)
#define MEMORY_SIZE (MEMORY_BLOCK*MEMORY_NUM)

    DecDemo *pDec;
    VideoDecoder *pVideoDec;
    //int bExitFlag = 0;
    int i, j;
    int times, state;
    char *pSrc = NULL;
    char *pDst = NULL;
    char *p = NULL;

    pDec = (DecDemo *)arg;
    pVideoDec = pDec->pVideoDec;

    pSrc = (char *)malloc(MEMORY_SIZE);
    if(pSrc == NULL)
    {
        logd(" DRAMcostTHread malloc fail ....pSrc ");
        goto DRAM_cost_exit;
    }
    pDst = (char *)malloc(MEMORY_SIZE);
    if(pDst == NULL)
    {
        logd(" DRAMcostTHread malloc fail ....pDst ");
        goto DRAM_cost_exit;
    }
    logd(" DRAM memory copy thread created .... ");
    times = 0;
    while(1)
    {
        char *s, *d;
        usleep(100);
        pthread_rwlock_wrlock(&pDec->thread.rwrock);
        //bExitFlag = pDec->thread.nEndofStream ;
        state = pDec->thread.state;
        pthread_rwlock_unlock(&pDec->thread.rwrock);
        if(state & DEMO_DECODER_EXIT)
        {
            logd(" DRAM COST THREAD EIXT..... ");
            break;
        }

        for(j = 0; j < MEMORY_NUM; j++)
        {
            s = pSrc + j*MEMORY_BLOCK;
            d = pDst + j*MEMORY_BLOCK;
            for(i = 0; i < 1080; i++)
            {
                int k;
                p = s;
                for(k = 0; k < MEMORY_NUM*10 && k*j < MEMORY_STRIDE; k++)
                    p[k*j] = rand()%128;
                memcpy(d, s, MEMORY_STRIDE);
                s += MEMORY_STRIDE;
                d += MEMORY_STRIDE;
            }
        }
    }
DRAM_cost_exit:
    if(pSrc)
        free(pSrc);
    if(pDst)
        free(pDst);
    pthread_exit(NULL);
    return 0;
}

void DemoHelpInfo(void)
{
    logd(" ==== CedarC linux decoder demo help start ===== ");
    logd(" -h or --help to show the demo usage");
    logd(" demo created by jilinglin, allwinnertech/AL3 ");
    logd(" email: jilinglin@allwinnertech.com ");
    logd(" ===== CedarC linux decoder demo help end ====== ");
}
static int  initParams(Dec_Params * params)
{
    int i = 0;
    for(i=0;i<DECODER_MAX_NUM;i++)
    {
        params->pInputFile[i] = calloc(DEMO_FILE_NAME_LEN, 1);
        if(params->pInputFile[i] == NULL)
        {
            loge(" input file. calloc memory fail. ");
            return 0;
        }
        params->pOutputFile[i] = calloc(DEMO_FILE_NAME_LEN, 1);
        if(params->pOutputFile[i] == NULL)
        {
            loge(" output file. calloc memory fail. ");
            free(params->pInputFile[i]);
            return 0;
        }
        params->pSaveShaFile[i] = calloc(DEMO_FILE_NAME_LEN, 1);
        if(params->pSaveShaFile[i] == NULL)
        {
            loge(" output file. calloc memory fail. ");
            free(params->pOutputFile[i]);
            free(params->pInputFile[i]);
            return 0;
        }
        params->pCompareShaFile[i] = calloc(DEMO_FILE_NAME_LEN, 1);
        if(params->pCompareShaFile[i] == NULL)
        {
            loge(" output file. calloc memory fail. ");
            free(params->pOutputFile[i]);
            free(params->pInputFile[i]);
            free(params->pSaveShaFile[i]);
            return 0;
        }

        params->nFinishNum[i] = 0x7fffffff;
        params->nSavePictureStartNumber[i] = 0xffffff;
        params->nDramCostThreadNum[i] = 0;
        params->nVeFreq[i] = 0;
        params->nCodecFormat[i] = VIDEO_FORMAT_H264 ;
        params->nOutputPixelFormat[i]= PIXEL_FORMAT_YUV_PLANER_420;
        params->nSavePictureNumber[i]= 0;
    }
    params->nDecoderNum = 1;
    return 1;
}

static int FreeParams(Dec_Params * params)
{
    int i =0;
    for(i=0;i<DECODER_MAX_NUM;i++)
    {
        if(params->pInputFile[i] != NULL)
            free(params->pInputFile[i]);
        if(params->pOutputFile[i] != NULL)
            free(params->pOutputFile[i]);
        if(params->pSaveShaFile[i] != NULL)
            free(params->pSaveShaFile[i]);
        if(params->pCompareShaFile[i] != NULL)
            free(params->pCompareShaFile[i]);
    }
    return 1;
}
void* ChannelThread(void* param)
{
    DecDemo *Decoder = (DecDemo *)param;
    int nRet = 0;
    int i=0, nDramCostThreadNum;
    pthread_t tdecoder, tparser, tdisplay;
    pthread_t dram[DRAM_COST_MAX_THREAD_NUM];
    long long endTime;

    DemoHelpInfo();
    i = Decoder->nChannel;
    //memset(&Decoder, 0, sizeof(DecDemo));
    Decoder->nFinishNum = Decoder->pParams.nFinishNum[i];
    Decoder->nDramCostThreadNum = Decoder->pParams.nDramCostThreadNum[i];
    Decoder->nSavePictureNumber = Decoder->pParams.nSavePictureNumber[i];
    Decoder->nSavePictureStartNumber = Decoder->pParams.nSavePictureStartNumber[i];
    Decoder->nOutputPixelFormat = Decoder->pParams.nOutputPixelFormat[i];
    Decoder->nCodecFormat = Decoder->pParams.nCodecFormat[i];
    Decoder->pInputFile = Decoder->pParams.pInputFile[i];
    Decoder->pOutputFile = Decoder->pParams.pOutputFile[i];
    Decoder->pSaveShaFile = Decoder->pParams.pSaveShaFile[i];
    Decoder->pCompareShaFile = Decoder->pParams.pCompareShaFile[i];

    if(Decoder->pSaveShaFile!= NULL && strcmp(Decoder->pSaveShaFile,"") != 0)
    {
        Decoder->pSaveShaFp = fopen(Decoder->pSaveShaFile, "wb");
        logd("open save sha file: %p",Decoder->pSaveShaFp);
    }
    if(Decoder->pCompareShaFile!= NULL && strcmp(Decoder->pCompareShaFile,"") != 0)
    {
        Decoder->pCompareShaFp= fopen(Decoder->pCompareShaFile, "rb");
        logd("open compare sha file: %p",Decoder->pCompareShaFp);
    }

    Decoder->pInputFileFp = fopen(Decoder->pInputFile, "r");

    logd("Decoder.pInputFileFp = %p, Decoder.pInputFile = %s", \
        Decoder->pInputFileFp,Decoder->pInputFile);
    Decoder->pParser = RawParserOpen(Decoder->pInputFileFp);
    if(Decoder->pParser == NULL)
    {
        loge("RawParserOpen failed");
        return 0;
    }
    nRet = initDecoder(Decoder);
    if(nRet != 0)
    {
        loge(" decoder demom initDecoder error ");
        return 0;
    }
    logd("decoder file: %s", Decoder->pInputFile);

    Decoder->totalTime = GetNowUs();

    nDramCostThreadNum = Decoder->nDramCostThreadNum;

    pthread_create(&tparser, NULL, parserThreadFunc, (void*)(Decoder));
    pthread_create(&tdecoder, NULL, DecodeThread, (void*)(Decoder));
    pthread_create(&tdisplay, NULL, displayPictureThreadFunc, (void*)(Decoder));
    for(i = 0; i < nDramCostThreadNum; i++)
    {
        pthread_create(&dram[i], NULL, DRAMcostTHread, (void*)(Decoder));
        logd(" creat dram memory copy thread[%d] ", i);
    }

    pthread_join(tparser, (void**)&nRet);
    pthread_join(tdecoder, (void**)&nRet);
    pthread_join(tdisplay, (void**)&nRet);
    for(i = 0; i < nDramCostThreadNum; i++)
        pthread_join(dram[i], (void**)&nRet);

    endTime = GetNowUs();
    Decoder->totalTime = endTime - Decoder->totalTime;
    loge(" demoDecoder finish.decode frame: %d, display frame: %d, cost %lld s ",
            Decoder->nDecodeFrameCount, Decoder->nDispFrameCount, Decoder->totalTime/(1000*1000));
    if(Decoder->pCompareShaFp != NULL)
        logd("******** nCompare-Sha-Error-Count = %d", Decoder->nCompareShaErrorCount);
    pthread_mutex_destroy(&Decoder->parserMutex);
    DestroyVideoDecoder(Decoder->pVideoDec);

    if(Decoder->pSaveShaFp != NULL)
        fclose(Decoder->pSaveShaFp);
    if(Decoder->pCompareShaFp != NULL)
        fclose(Decoder->pCompareShaFp);

    logd(" demo decoder exit successful");
    CdcMemClose(Decoder->memops);

    if(Decoder->pParser)
        RawParserClose(Decoder->pParser);

    return 0;
}


int main(int argc, char** argv)
{
    pthread_t tchanelDecoder[DECODER_MAX_NUM];
    DecDemo  *pDecoder[DECODER_MAX_NUM] = {NULL};
    int nRet = 0;
    int i = 0;
    int nDecoderNum = 1;
    int nFindParserStart= 0;
    Dec_Params *params;
    int  nCurIndex = 1;
    int nStart = 0;
    int nStreamDataLen = -1;
    ARGUMENT_T arg = INVALID;
    DemoHelpInfo();

    params = calloc(sizeof(DecDemo), 1);
    if(params == NULL)
    {
        loge("malloc for params failed");
        return 0;
    }

    if((initParams(params)) == 0)
    {
        return 0;
    }

    if(argc >= 2)
    {
        while(nCurIndex < argc)
        {
            for(i = nCurIndex; i <argc; i++)
            {
                arg = GetArgument(argv[i]);
                if(arg != INVALID)
                {
                    nFindParserStart = 1;
                    break;
                }
            }

            logv("nFindParserStart = %d, i = %d, arg = %d",nFindParserStart, i, arg);

            if(nFindParserStart ==1)
            {
                nFindParserStart = 0;
                nStart = i;
                for(i +=2; i <argc; i++)
                {
                    arg = GetArgument(argv[i]);
                    if(arg != INVALID)
                    {
                        nFindParserStart = 1;
                        break;
                    }
                }

                if(nFindParserStart == 1)
                {
                    nStreamDataLen = i - nStart;
                }
                else
                {
                    nStreamDataLen = argc - nStart;
                }
            }
            else
            {
                return 0;
            }

            if(nStreamDataLen > DECODER_MAX_NUM + 1)
            {
                return 0;
            }
            logv("nCurIndex = %d, nStreamDataLen = %d",nCurIndex, nStreamDataLen);
            ParseArgument(params, argv, nCurIndex,nStreamDataLen);
            nCurIndex = nStreamDataLen + nCurIndex;
            nStreamDataLen = 0;
        }
    }

    nDecoderNum = params->nDecoderNum;
    if(nDecoderNum <= 0)
        nDecoderNum = 1;
    else if(nDecoderNum > DECODER_MAX_NUM)
        nDecoderNum = DECODER_MAX_NUM;

    loge("nDecoderNum = %d",nDecoderNum);

    for(i = 0; i < nDecoderNum; i++)
    {
        pDecoder[i] = calloc(sizeof(DecDemo), 1);
        if(pDecoder[i] == NULL)
        {
            loge("malloc for pDecoder failed");
            goto OUT;
        }
        //pDecoder[i]->pParams = params;
        memcpy(&pDecoder[i]->pParams,params,sizeof(Dec_Params));
        pDecoder[i]->nChannel = i;
    }

    for(i = 0; i < nDecoderNum; i++)
    {
        pthread_create(&tchanelDecoder[i], NULL, ChannelThread, (void*)(pDecoder[i]));
    }

    for(i = 0; i < nDecoderNum; i++)
    {
        pthread_join(tchanelDecoder[i], (void**)&nRet);
    }

OUT:

    FreeParams(params);
    for(i = 0; i < nDecoderNum; i++)
    {
        if(pDecoder[i])
            free(pDecoder[i]);
    }

    return 0;
}

