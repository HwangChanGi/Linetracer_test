#include "opencv2/opencv.hpp"
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "dxl.hpp"
using namespace cv;
using namespace std;
bool ctrl_c_pressed;
void ctrlc(int) //SIGINT �ñ׳�(Ctrl+C)�� ���޵ɶ� ������ �ñ׳��ڵ鷯 �Լ�
{
    ctrl_c_pressed = true;
}
int main()
{
    string src = "nvarguscamerasrc sensor-id=0 ! video/x-raw(memory:NVMM), width=(int)640, \
    height=(int)360, format=(string)NV12 ! \
    nvvidconv flip-method=0 ! video/x-raw, width=(int)640, height=(int)360, \
    format=(string)BGRx ! videoconvert ! \
    video/x-raw, format=(string)BGR !appsink";
    string dst1 = "appsrc ! videoconvert ! video/x-raw, format=BGRx ! nvvidconv ! nvv4l2h264enc \
    insert-sps-pps=true ! h264parse ! rtph264pay pt=96 ! udpsink host=203.234.58.159 \
    port=8001 sync=false";
    string dst2 = "appsrc ! videoconvert ! video/x-raw, format=BGRx ! nvvidconv ! nvv4l2h264enc \
    insert-sps-pps=true ! h264parse ! rtph264pay pt=96 ! udpsink host=203.234.58.159 \
    port=8002 sync=false";
    string dst3 = "appsrc ! videoconvert ! video/x-raw, format=BGRx ! nvvidconv ! nvv4l2h264enc \
    insert-sps-pps=true ! h264parse ! rtph264pay pt=96 ! udpsink host=203.234.58.159 \
    port=8003 sync=false";

    struct timeval start, end;
    double diff;

    VideoCapture source(src, CAP_GSTREAMER);
    if (!source.isOpened()) { cerr << "Video error" << endl; return -1; }

    VideoWriter writer1(dst1, 0, (double)30, cv::Size(640, 360), true);
    if (!writer1.isOpened()) { cerr << "Writer open failed!" << endl; return -1; }
    VideoWriter writer2(dst2, 0, (double)30, cv::Size(640, 90), true);
    if (!writer2.isOpened()) { cerr << "Writer open failed!" << endl; return -1; }
    VideoWriter writer3(dst3, 0, (double)30, cv::Size(640, 90), true);
    if (!writer3.isOpened()) { cerr << "Writer open failed!" << endl; return -1; }

    Dxl mx;
    int vel1 = 0, vel2 = 0, y = 0;
    signal(SIGINT, ctrlc);
    if (!mx.open()) { cout << "dynamixel open error" << endl; return -1; } //��ġ����

    Mat frame, resize_img, roi_img, gray, thre;

    Point2d prevpt(320, 45);    // ���� ��ü �߽� ��ǥ
    Point2d cpt;
    int error = 0, minlb;
    double ptdistance, threshdistance;
    vector<double> mindistance;

    while (true) {
        gettimeofday(&start, NULL);

        source >> frame;
        if (frame.empty()) {
            cerr << "frame empty!" << endl;
            break;
        }

        resize(frame, resize_img, Size(640, 360));
        roi_img = resize_img(Rect(0, 270, 640, 90));

        cvtColor(roi_img, gray, COLOR_BGR2GRAY);

        gray = gray + 100 - mean(gray)[0];      // �̹��� ��� ��Ⱚ

        threshold(gray, thre, 130, 255, THRESH_BINARY);

        Mat labels, stats, centroids;
        int Num_Labels = connectedComponentsWithStats(thre, labels, stats, centroids);

        Mat colorThre;
        cvtColor(thre, colorThre, COLOR_GRAY2BGR);

        if (Num_Labels > 1) {       // ������ ��ü�� 2�� �̻��� ��� ����
            // ��� ��ü�� �߽� ��ǥ centroids �� ���� ������ ���� ��ü �߽� ��ǥ prevpt ���� �Ÿ� ��� mindistance ���� 
            for (int i = 1; i < Num_Labels; i++) {
                double* p = centroids.ptr<double>(i);
                ptdistance = abs(p[0] - prevpt.x);
                mindistance.push_back(ptdistance);
            }

            // ���� ����� �߽� ��ǥ�� �Ÿ��� ��� mindistance ���� ���� �� ã�� threshdistance���� �ش� �� �ε��� minlb ã��
            threshdistance = *min_element(mindistance.begin(), mindistance.end());
            minlb = min_element(mindistance.begin(), mindistance.end()) - mindistance.begin();

            //���� ����� �߽� ��ǥ ���� centroids���� minlb + 1 �ε����� �ش��ϴ� ��ü�� �߽� ��ǥ ���� cpt �Ҵ�
            cpt = Point2d(centroids.at<double>(minlb + 1, 0), centroids.at<double>(minlb + 1, 1));

            //�Ÿ��� ���� �Ӱ谪���� ũ�ٸ� ���� �߽� ��ǥ�� ���� ���� �Ӱ谪 ���� ũ�ٸ� cpt�� ���� �߽���ǥ prevpt�� ����
            if (threshdistance > 100) {
                cpt = prevpt;
            }
            // ���� ����
            mindistance.clear();

            // ���� ��ü ���� �ٿ�� �ڽ� �׸���
            int left = stats.at<int>(minlb + 1, CC_STAT_LEFT);
            int top = stats.at<int>(minlb + 1, CC_STAT_TOP);
            int width = stats.at<int>(minlb + 1, CC_STAT_WIDTH);
            int height = stats.at<int>(minlb + 1, CC_STAT_HEIGHT);
            rectangle(colorThre, Point(left, top), Point(left + width, top + height), Scalar(0, 0, 255), 1);

            // �ٸ� ��ü ������ �ٿ�� �ڽ��� �߽� ��ǥ �׸���
            for (int i = 1; i < Num_Labels; i++) {
                if (i != minlb + 1) {
                    int otherLeft = stats.at<int>(i, CC_STAT_LEFT);
                    int otherTop = stats.at<int>(i, CC_STAT_TOP);
                    int otherWidth = stats.at<int>(i, CC_STAT_WIDTH);
                    int otherHeight = stats.at<int>(i, CC_STAT_HEIGHT);
                    rectangle(colorThre, Point(otherLeft, otherTop), Point(otherLeft + otherWidth, otherTop + otherHeight), Scalar(255, 0, 0), 1);

                    double* p = centroids.ptr<double>(i);
                    Point2d otherCpt(p[0], p[1]);
                    circle(colorThre, otherCpt, 2, Scalar(255, 0, 0), -1);
                }
            }
            //int imageCenterX = colorThre.cols / 2;
            //int lineCenterX = static_cast<int>(centroids.at<double>(minlb + 1, 0));
            //error = imageCenterX - lineCenterX;
            error = cpt.x - roi_img.cols / 2;
        }

        else { cpt = prevpt; }

        prevpt = cpt;

        // ���� ��ü ������ �߽� ��ǥ �׸���
        circle(colorThre, cpt, 2, Scalar(0, 0, 255), 2);

        writer1 << resize_img;
        writer2 << roi_img;
        writer3 << colorThre;

        if (mx.kbhit()) //Ű�����Է� üũ
        {
            char c = mx.getch(); //Ű�Է� �ޱ�
            if (c == 'q') break;
            else if (c == 's') {
                vel1 = 50;
                vel2 = -50;
                y++;
            }
        }

        if (error > 0 && y > 0) {
            vel1 = 50 + 0.2 * error;
        }
        else if (error < 0 && y>0) {
            vel2 = -50 + 0.2 * error;
        }
        if (!mx.setVelocity(vel1, vel2)) { //�ӵ���� ����

            cout << "setVelocity error" << endl;
            return -1;
        }

        gettimeofday(&end, NULL);
        diff = end.tv_sec + end.tv_usec / 1000000.0 - start.tv_sec - start.tv_usec / 1000000.0;
        cout << "error: " << error;
        cout << "   lvel: " << vel1 << "    rvel: " << vel2;
        cout << "   time: " << diff << endl;

        if (waitKey(30) == 27) break;
        if (ctrl_c_pressed) { // Ctrl+C�� ������ ���ѷ����� break��
            break;
        }

    }
    mx.close(); //��ġ�ݱ�
    return 0;
}
