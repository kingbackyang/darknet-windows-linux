#include "network.h"
#include "detection_layer.h"
#include "region_layer.h"
#include "cost_layer.h"
#include "utils.h"
#include "parser.h"
#include "box.h"
#include "image.h"
#include "demo.h"
#ifdef WIN32
#include <time.h>
#include <winsock.h>
#include "gettimeofday.h"
#else
#include <sys/time.h>
#endif
#define FRAMES 3

#ifdef OPENCV
#include "opencv2/highgui/highgui_c.h"
#include "opencv2/imgproc/imgproc_c.h"
#include "opencv2/core/version.hpp"
#ifndef CV_VERSION_EPOCH
#include "opencv2/videoio/videoio_c.h"
#endif
#include "http_stream.h"
image get_image_from_stream(CvCapture *cap);

static char **demo_names;
static image **demo_alphabet;
static int demo_classes;

static float **probs;
static box *boxes;
static network net;
static image in_s ;
static image det_s;
static CvCapture * cap;
static int cpp_video_capture = 0;
static float fps = 0;
static float demo_thresh = 0;
static int demo_ext_output = 0;

static float *predictions[FRAMES];
static int demo_index = 0;
static image images[FRAMES];
static IplImage* ipl_images[FRAMES];
static float *avg;

void draw_detections_cv(IplImage* show_img, int num, float thresh, box *boxes, float **probs, char **names, image **alphabet, int classes);
void draw_detections_cv_v3(IplImage* show_img, detection *dets, int num, float thresh, char **names, image **alphabet, int classes, int ext_output);
void show_image_cv_ipl(IplImage *disp, const char *name);
image get_image_from_stream_resize(CvCapture *cap, int w, int h, int c, IplImage** in_img, int cpp_video_capture, int dont_close);
image get_image_from_stream_letterbox(CvCapture *cap, int w, int h, int c, IplImage** in_img, int cpp_video_capture, int dont_close);
int get_stream_fps(CvCapture *cap, int cpp_video_capture);
IplImage* in_img;
IplImage* det_img;
IplImage* show_img;

static int flag_exit;
static int letter_box = 0;
//MODIFIED
int n_tube = 0;
int n_tube_pre=0;
int stopcount = 0;
int stopcount_pre=0;
int drop_state = -1;
int init_state=1;
int saveFrame=-1;
int saveFramePre=-1;
int startSaveBrand=0;
//Brand--------------
int saveBrand=-1;
int saveBrandPre=-1;
int stopcount_b = 0;
int stopcount_b_pre=0;
int n_brand=0;
int n_brand_pre=0;
int drop_state_b=-1;
int init_state_b=1;
int wait_flag_b=0;
int saveBrandCount=0;
int cnt=0;
#define TH_B 25
#define SAVETH_B 9
//Brand End----------
int n_person=-1;
int wait_flag=0;
int saveFrameCount=0;
CvSize size;
int src_fps = 25;
char time_tmp[64]="";
#define SAVETH 15
#define TH 50

char* getTime()
{
    time_t timep;
    time(&timep);
    //timep=timep+8*3600; //transform timezone
    //char tmp[64]={'\0'};
    strftime(time_tmp, sizeof(time_tmp), "%Y-%m-%d %H:%M:%S", localtime(&timep));
/*
    struct tm* t=gmtime(&timep);

    sprintf(time_tmp,"%d-%02d-%02d %02d:%02d:%02d",
            t->tm_year + 1900,
            t->tm_mon + 1,
            t->tm_mday,
            t->tm_hour,
            t->tm_min,
            t->tm_sec
            );
*/
    return time_tmp;
}
//END MODIFY
void *fetch_in_thread(void *ptr)
{
    //in = get_image_from_stream(cap);
    int dont_close_stream = 0;    // set 1 if your IP-camera periodically turns off and turns on video-stream
    if(letter_box)
        in_s = get_image_from_stream_letterbox(cap, net.w, net.h, net.c, &in_img, cpp_video_capture, dont_close_stream);
    else
        in_s = get_image_from_stream_resize(cap, net.w, net.h, net.c, &in_img, cpp_video_capture, dont_close_stream);
    if(!in_s.data){
        //error("Stream closed.");
        printf("Stream closed.\n");
        flag_exit = 1;
        return EXIT_FAILURE;
    }
    //in_s = resize_image(in, net.w, net.h);

    return 0;
}

void *detect_in_thread(void *ptr)
{
    //int *res;
    float nms = .45;    // 0.4F
    //int saveFrame=-1;
    layer l = net.layers[net.n-1];
    float *X = det_s.data;
    float *prediction = network_predict(net, X);

    memcpy(predictions[demo_index], prediction, l.outputs*sizeof(float));
    mean_arrays(predictions, FRAMES, l.outputs, avg);
    l.output = avg;

    free_image(det_s);

    int nboxes = 0;
    detection *dets = NULL;
    if (letter_box)
        dets = get_network_boxes(&net, in_img->width, in_img->height, demo_thresh, demo_thresh, 0, 1, &nboxes, 1); // letter box
    else
        dets = get_network_boxes(&net, det_s.w, det_s.h, demo_thresh, demo_thresh, 0, 1, &nboxes, 0); // resized
    //if (nms) do_nms_obj(dets, nboxes, l.classes, nms);    // bad results
    if (nms) do_nms_sort(dets, nboxes, l.classes, nms);


    printf("\033[2J");
    printf("\033[1;1H");
    printf("\nFPS:%.1f\n",fps);
    printf("Objects:\n\n");


    ipl_images[demo_index] = det_img;
    det_img = ipl_images[(demo_index + FRAMES / 2 + 1) % FRAMES];
    demo_index = (demo_index + 1)%FRAMES;

    draw_detections_cv_v3(det_img, dets, nboxes, demo_thresh, demo_names, demo_alphabet, demo_classes, demo_ext_output);//add saveFrame
	//saveFrame=draw_detections_cv_v3(det_img, dets, nboxes, demo_thresh, demo_names, demo_alphabet, demo_classes, demo_ext_output);//add saveFrame
    //MODIFIED 
    printf("saveFrame:%d\n",saveFrame);	
    n_tube_pre=n_tube;
    if (stopcount_pre>TH)
        init_state=0;
    if (saveFrame && init_state)
    {
        wait_flag=1;
        init_state=0;
        //n_tube++;
    }

    /*if (stopcount_pre>25 && saveFrame)
    {
        init_state=0;
        n_tube++;   
    }*/
    if (stopcount_pre>TH && saveFrame)
        wait_flag=1;
    if (wait_flag && stopcount_pre <= TH && (saveFrame==1) && (saveFramePre==1))
    {
        saveFrameCount++;
        if (saveFrameCount>=SAVETH)
        {
            init_state=0;
            n_tube++;
            saveFrameCount=0;
            wait_flag=0;
        }
    }


	if (saveFrame != 1)
		stopcount++;
    else
        stopcount=0;
    /*
    if (stopcount_pre>25 && saveFrame)
        n_tube++;    
        */
	if (stopcount > TH) {
		printf("DROP!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
       // cvReleaseImage(&det_img);
        drop_state=1;
        wait_flag=0;
        saveFrameCount=0;
    }
    else
        drop_state=0;
    stopcount_pre=stopcount;
    saveFramePre=saveFrame;
    //n_tube_pre=n_tube;

    printf("stopcount:%d\n",stopcount );
    //printf("n_tube:%d\n",n_tube );
    printf("n_person:%d\n",n_person);
	//END MODIFY
	//-----------------------------------------------------------------------------
    //-----------------------------------------------------------------------------
    //----------------------saveBrand----------------------------------------------
    printf("saveBrand:%d\n",saveBrand); 
    n_brand_pre=n_brand;
    if (stopcount_b_pre>TH_B)
        init_state_b=0;
    if (saveBrand && init_state_b)
    {
        wait_flag_b=1;
        init_state_b=0;
    }
    if (stopcount_b_pre>TH && saveBrand)
        wait_flag_b=1;
    if (wait_flag_b && stopcount_b_pre <= TH_B && (saveBrand==1) && (saveBrandPre==1))
    {
        saveBrandCount++;
        if (saveBrandCount>=SAVETH_B)
        {
            init_state_b=0;
            n_brand++;
            saveBrandCount=0;
            wait_flag_b=0;
        }
    }
    if (saveBrand != 1)
        stopcount_b++;
    else
        stopcount_b=0;
    if (stopcount > TH_B) {
        drop_state_b=1;
        wait_flag_b=0;
        saveBrandCount=0;
    }
    else
        drop_state_b=0;
    stopcount_b_pre=stopcount_b;
    saveBrandPre=saveBrand;
    printf("stopcount_b:%d\n",stopcount_b);
    //printf("n_brand:%d\n",n_brand );
    //-----------------------------------------------------------------------------
    //------------------------------------------------------------------------------
    free_detections(dets, nboxes);

    return 0;
}

double get_wall_time()
{
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

void demo(char *cfgfile, char *weightfile, float thresh, float hier_thresh, int cam_index, const char *filename, char **names, int classes,
    int frame_skip, char *prefix, char *out_filename, int http_stream_port, int dont_show, int ext_output, int gpu)
{


    //skip = frame_skip;
    image **alphabet = load_alphabet();
    int delay = frame_skip;
    demo_names = names;
    demo_alphabet = alphabet;
    demo_classes = classes;
    demo_thresh = thresh;
    demo_ext_output = ext_output;
    printf("Demo\n");
#ifdef GPU
	cuda_set_device(gpu);
#endif
    net = parse_network_cfg_custom(cfgfile, 1);    // set batch=1
    if(weightfile){
        load_weights(&net, weightfile);
    }
    //set_batch_network(&net, 1);
    fuse_conv_batchnorm(net);
    srand(2222222);

    if(filename){
        printf("video file: %s\n", filename);
//#ifdef CV_VERSION_EPOCH    // OpenCV 2.x
//        cap = cvCaptureFromFile(filename);
//#else                    // OpenCV 3.x
        cpp_video_capture = 1;
        cap = get_capture_video_stream(filename);
//#endif
    }else{
        printf("Webcam index: %d\n", cam_index);
//#ifdef CV_VERSION_EPOCH    // OpenCV 2.x
//        cap = cvCaptureFromCAM(cam_index);
//#else                    // OpenCV 3.x
        cpp_video_capture = 1;
        cap = get_capture_webcam(cam_index);
//#endif
    }

    if (!cap) {
#ifdef WIN32
        printf("Check that you have copied file opencv_ffmpeg330_64.dll to the same directory where is darknet.exe \n");
#endif
        error("Couldn't connect to webcam.\n");
    }

    layer l = net.layers[net.n-1];
    int j;

    avg = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < FRAMES; ++j) predictions[j] = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < FRAMES; ++j) images[j] = make_image(1,1,3);

    boxes = (box *)calloc(l.w*l.h*l.n, sizeof(box));
    probs = (float **)calloc(l.w*l.h*l.n, sizeof(float *));
    for(j = 0; j < l.w*l.h*l.n; ++j) probs[j] = (float *)calloc(l.classes, sizeof(float *));

    flag_exit = 0;

    pthread_t fetch_thread;
    pthread_t detect_thread;

    fetch_in_thread(0);
    det_img = in_img;
    det_s = in_s;

    fetch_in_thread(0);
    detect_in_thread(0);
    det_img = in_img;
    det_s = in_s;

    for(j = 0; j < FRAMES/2; ++j){
        fetch_in_thread(0);
        detect_in_thread(0);
        det_img = in_img;
        det_s = in_s;
    }

    int count = 0;
    if(!prefix && !dont_show){
        cvNamedWindow("Demo", CV_WINDOW_NORMAL);
        cvMoveWindow("Demo", 0, 0);
        cvResizeWindow("Demo", 1352, 1013);
    }

    CvVideoWriter* output_video_writer = NULL;
    //MODIFIED
    FILE *f_result;
    f_result = fopen("./result.txt", "w");
    //END MODIFY
    //OUTPUT COMPLETED VIDEO FILES
    
    CvVideoWriter* output_complete_video_writer = NULL;    // cv::VideoWriter output_video;
    if (out_filename && !flag_exit)
    {
        char out_filename_complete[50];
        //CvSize size;//gloable
        size.width = det_img->width, size.height = det_img->height;
        //int src_fps = 25;//gloable
        src_fps = get_stream_fps(cap, cpp_video_capture);

        //const char* output_name = "test_dnn_out.avi";
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('H', '2', '6', '4'), src_fps, size, 1);
        sprintf(out_filename_complete, "%s.mp4",out_filename);
        output_complete_video_writer = cvCreateVideoWriter(out_filename_complete, CV_FOURCC('x', '2', '6', '4'), src_fps, size, 1);
        //output_complete_video_writer = cvCreateVideoWriter(out_filename_complete, CV_FOURCC('D', 'I', 'V', 'X'), src_fps, size, 1);
        printf("COMPLETED VIDEO WRITER CREATED!!");
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('M', 'J', 'P', 'G'), src_fps, size, 1);
        //output_complete_video_writer = cvCreateVideoWriter(out_filename_complete, CV_FOURCC('M', 'P', '4', 'V'), src_fps, size, 1);
        //output_complete_video_writer = cvCreateVideoWriter(out_filename_complete, CV_FOURCC('M', 'P', '4', '2'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('X', 'V', 'I', 'D'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('W', 'M', 'V', '2'), src_fps, size, 1);
    }
    //END DEFINE

    double before = get_wall_time();

    while(1){
        ++count;
        if(1){
            if(pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
            if(pthread_create(&detect_thread, 0, detect_in_thread, 0)) error("Thread creation failed");

            if(!prefix){
                if (!dont_show) {
                    show_image_cv_ipl(show_img, "Demo");
                    int c = cvWaitKey(1);
                    if (c == 10) {
                        if (frame_skip == 0) frame_skip = 60;
                        else if (frame_skip == 4) frame_skip = 0;
                        else if (frame_skip == 60) frame_skip = 4;
                        else frame_skip = 0;
                    }
                    else if (c == 27 || c == 1048603) // ESC - exit (OpenCV 2.x / 3.x)
                    {
                        flag_exit = 1;
                    }
                }
            }else{
                char buff[256];
                sprintf(buff, "%s_%08d.jpg", prefix, count);
                cvSaveImage(buff, show_img, 0);
                //save_image(disp, buff);
            }

            // if you run it with param -http_port 8090  then open URL in your web-browser: http://localhost:8090
            if (http_stream_port > 0 && show_img) {
                //int port = 8090;
                int port = http_stream_port;
                int timeout = 20000;
                int jpeg_quality = 10;    // 1 - 100
                send_mjpeg(show_img, port, timeout, jpeg_quality);
            }

            // save complete video file -writeframes
            /*
            if (output_complete_video_writer && show_img) {
                cvWriteFrame(output_complete_video_writer, show_img);
                //printf("\n cvWriteFrame \n");
            } */
            //end write frames
			
			//MODIFIED!! save video file------------------------------------------------------------------------------
            //printf("1\n");
            //CvVideoWriter* output_video_writer = NULL;    // cv::VideoWriter output_video;
            //printf("2\n");
            printf("n_tube:%d\n",n_tube);
            printf("n_tube_pre:%d\n",n_tube_pre);
            //if ((out_filename && !flag_exit && n_tube==1)||(out_filename && !flag_exit && n_tube - n_tube_pre > 0))
            if (out_filename && !flag_exit && n_tube - n_tube_pre > 0)
            {
                //printf("3\n");
                /*
                if (output_video_writer) {
                    cvReleaseVideoWriter(&output_video_writer);
                    //write time and n_tube_pre
                    char *e_time = getTime();
                    fprintf(f_result, "%s/", e_time);
                    fflush(f_result);
                    fprintf(f_result, "%d\n", n_tube_pre);
                    fflush(f_result);
                    printf("e_time:%s\n",e_time);
                }*/

                output_video_writer = NULL; 
                char out_filenames[50];
                //printf("3.1\n");
                //out_filenames = out_filename;
                //printf("3.1.1\n");
                sprintf(out_filenames, "%s-%d.mp4",out_filename,n_tube);
                //printf("3.2\n");
                //CvSize size;
                //size.width = det_img->width, size.height = det_img->height;
                //int src_fps = 25;
               // printf("3.3\n");
                //src_fps = get_stream_fps(cap, cpp_video_capture);
                //output_video_writer = cvCreateVideoWriter(out_filenames, CV_FOURCC('D', 'I', 'V', 'X'), src_fps, size, 1);
                output_video_writer = cvCreateVideoWriter(out_filenames, CV_FOURCC('X', '2', '6', '4'), src_fps, size, 1);
                //output_video_writer = cvCreateVideoWriter(out_filenames, CV_FOURCC('M', 'P', '4', 'V'), src_fps, size, 1);
                //output_video_writer = cvCreateVideoWriter(out_filenames, CV_FOURCC('M', 'P', '4', '2'), src_fps, size, 1);
                printf("VIDEO FILE CREATED!!!!!\nsrc_fps:%d\n",src_fps);
                //write time and n_peson to file 
                char *s_time = getTime();
                fprintf(f_result, "%d/", n_person);
                fflush(f_result);
                fprintf(f_result, "%s/", s_time);
                fflush(f_result);
                printf("s_time:%s\n",s_time );
               // printf("4\n");

            }


            // save complete video file -writeframes
            
            if (output_complete_video_writer && show_img) {
                cvWriteFrame(output_complete_video_writer, show_img);
                //printf("\n cvWriteFrame \n");
            } 
            //end write frames
            //printf("5\n");
			//if (output_video_writer && show_img && drop_state==0 && n_tube==n_tube_pre) {
            if (output_video_writer && show_img && drop_state==0 && n_tube==n_tube_pre) {
               // printf("6\n");
				cvWriteFrame(output_video_writer, show_img);
				printf("\n cvWriteFrame \n");
			}

            if (drop_state==1)
            {
                if (output_video_writer) {
                    cvReleaseVideoWriter(&output_video_writer);
                    //write time and n_tube_pre
                    char *e_time = getTime();
                    fprintf(f_result, "%s/", e_time);
                    fflush(f_result);
                    fprintf(f_result, "%d\n", n_tube_pre);
                    fflush(f_result);
                    printf("e_time:%s\n",e_time);
                }
            }
/*
			if (drop_state==1) {
				cvReleaseVideoWriter(&output_video_writer);//WRITING DONE AND CLOSE!!
				printf("output_video_writer REopening!! \n");

			    CvVideoWriter* output_video_writer = NULL;    // cv::VideoWriter output_video;
    			if (out_filename && !flag_exit)
    			{
    				char *out_filenames;
    				out_filenames = out_filename;
    				strcat(out_filenames, (char*)n_tube);
    				CvSize size;
    				size.width = det_img->width, size.height = det_img->height;
    				int src_fps = 25;
    				src_fps = get_stream_fps(cap, cpp_video_capture);
    				output_video_writer = cvCreateVideoWriter(out_filenames, CV_FOURCC('D', 'I', 'V', 'X'), src_fps, size, 1);

                }
                else
                    printf("UNABLE to REopen!!\n");
            }*/
            //----------------------------saveBrand----------------------------------------------------
            //-----------------------------------------------------------------------------------------
           // int startSaveBrand=0;
            //int cnt=0;              
            printf("cnt:%d\n",cnt);
            char brand_filenames[256];
            printf("n_brand:%d\n",n_brand);
            printf("n_brand_pre:%d\n",n_brand_pre);
            printf("StartSaveBrand:%d\n",startSaveBrand);
            if (out_filename&&!flag_exit && n_brand - n_brand_pre > 0)
            {
                startSaveBrand=1;
                cnt=0;
                printf("Start saveBrand!!!!!!!!!\n");
            }
            if (startSaveBrand && show_img && drop_state_b==0 && saveBrand && n_brand==n_brand_pre && cnt<5) {
                //cvWriteFrame(output_video_writer, show_img);
                cnt++;
                sprintf(brand_filenames, "%s_Brand%d_%d.jpg",out_filename,n_brand,cnt);
                cvSaveImage(brand_filenames, show_img, 0);
                printf("\n cvSaveImage \n");
            }
            if (drop_state_b&&startSaveBrand)
                startSaveBrand=0;

            //--------------------------------saveBrand End---------------------------------------
            //-------------------------------------------------------------------------------------
			//MODIFIED END--------------------------------------------------------------------------------------------


            cvReleaseImage(&show_img);

            pthread_join(fetch_thread, 0);
            pthread_join(detect_thread, 0);

            if (flag_exit == 1) break;
            //printf("delay:%d\n",delay);
            //printf("frame_skip:%d\n",frame_skip );
            if(delay == 0){
                show_img = det_img;
            }
            det_img = in_img;
            det_s = in_s;
        }else {
            fetch_in_thread(0);
            det_img = in_img;
            det_s = in_s;
            detect_in_thread(0);

            show_img = det_img;
            if (!dont_show) {
                show_image_cv_ipl(show_img, "Demo");
                cvWaitKey(1);
            }
            cvReleaseImage(&show_img);
        }
        --delay;
        if(delay < 0){
            delay = frame_skip;

            double after = get_wall_time();
            float curr = 1./(after - before);
            fps = curr;
            before = after;
        }
    //n_tube_pre=n_tube;
    }
    printf("input video stream closed. \n");
    if (output_video_writer) {
        cvReleaseVideoWriter(&output_video_writer);
        printf("output_video_writer closed. \n");
    }
    if (output_complete_video_writer) {
        cvReleaseVideoWriter(&output_complete_video_writer);
        printf("output_complete_video_writer closed. \n");
    }

    // free memory
    cvReleaseImage(&show_img);
    cvReleaseImage(&in_img);
    free_image(in_s);

    free(avg);
    for (j = 0; j < FRAMES; ++j) free(predictions[j]);
    for (j = 0; j < FRAMES; ++j) free_image(images[j]);

    for (j = 0; j < l.w*l.h*l.n; ++j) free(probs[j]);
    free(boxes);
    free(probs);

    free_ptrs(names, net.layers[net.n - 1].classes);

    int i;
    const int nsize = 8;
    for (j = 0; j < nsize; ++j) {
        for (i = 32; i < 127; ++i) {
            free_image(alphabet[j][i]);
        }
        free(alphabet[j]);
    }
    free(alphabet);

    free_network(net);
}
#else
void demo(char *cfgfile, char *weightfile, float thresh, float hier_thresh, int cam_index, const char *filename, char **names, int classes,
    int frame_skip, char *prefix, char *out_filename, int http_stream_port, int dont_show, int ext_output)
{
    fprintf(stderr, "Demo needs OpenCV for webcam images.\n");
}
#endif

