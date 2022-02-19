#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>

Window findWindow(Display* display, Window window, const char* title) {
  // check if current window name matches
  char *windowTitle = nullptr;
  if (XFetchName(display, window, &windowTitle) > 0) {
    int cmp = strcmp(title, windowTitle);
    XFree(windowTitle);
    if (cmp == 0) {
      return window;
    }
  }

  // search recursively
  Window result = 0;
  Window root, parent, *children;
  unsigned int childCount = 0;
  if (XQueryTree(display, window, &root, &window, &children, &childCount) != 0) {
    for (unsigned int i = 0; i < childCount; ++i) {
      Window childWindow = findWindow(display, children[i], title);

      if (childWindow != 0) {
        result = childWindow;
        break;
      }
    }
    XFree(children);
  }
  return result;
}

bool keyIsPressed(Display* display, KeySym ks) {
  char keys[32];
  XQueryKeymap(display, keys);
  KeyCode kc2 = XKeysymToKeycode(display, ks);
  return !!(keys[kc2 >> 3] & (1 << (kc2 & 7)));
}

void windowToMat(Display* display, Window window, int width, int height, cv::Mat& output) {
  XImage* img = XGetImage(display, window, 0, 0 , width, height, AllPlanes, ZPixmap);
  memcpy(&output.data[0], img->data, width * height * 4);
  XDestroyImage(img);
}

int getProgress(cv::Mat windowImg) {
  int x1 = 0.5435244161358811 * (float) windowImg.cols - 131.78343949044586;
  int y1 = 0.9866220735785953 * (float) windowImg.rows - 208.52842809364552;
  int x2 = 0.4946921443736730 * (float) windowImg.cols +  95.09554140127386;
  int y2 = 0.5828729281767956 * (float) windowImg.rows + 211.0469613259669;
  auto rect = cv::Rect(x1, y1, x2 - x1, y2 - y1);
  cv::rectangle(windowImg, rect, cv::Scalar(255, 0, 0));
  cv::Mat progressImg = windowImg(rect);
  //cv::imwrite("window.png", windowImg);
  //cv::imwrite("progress.png", progressImg);
  cv::Mat grayImg;
  cv::cvtColor(progressImg, grayImg, cv::COLOR_BGR2GRAY);
  cv::Mat threshImage;
  cv::threshold(grayImg, threshImage, 180, 255, cv::THRESH_BINARY);
  double minVal, maxVal;
  cv::Point minLoc, maxLoc;
  cv::minMaxLoc(threshImage, &minVal, &maxVal, &minLoc, &maxLoc);
  //cv::namedWindow("window", 1);
  //cv::namedWindow("gray", 1);
  //cv::namedWindow("progressImage", 1);
  //cv::namedWindow("Output", 1);
  //cv::imshow("window", windowImg);
  //cv::imshow("gray", grayImg);
  //cv::imshow("progressImage", progressImg);
  //cv::imshow("Output", threshImage);
  //cv::waitKey(0);
  return 100 * maxLoc.x / progressImg.cols;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "you must pass a percent number!" << std::endl;
    return 1;
  }
  int endProgress = 0;
  try {
    endProgress = std::stoi(argv[1]);
  } catch (std::exception& e) {
    std::cerr << "you must enter a valid number!" << std::endl;
    return 2;
  }
  if (endProgress < 0 || endProgress > 99) {
    std::cerr << "number must be between 0 and 99!" << std::endl;
    return 3;
  }
  Display* display = XOpenDisplay(nullptr);
  Window rootWindow = XDefaultRootWindow(display);
  Window gameWindow = findWindow(display, rootWindow, "Guild Wars 2");
  XWindowAttributes attributes = {0};
  XGetWindowAttributes(display, gameWindow, &attributes);
  int windowWidth = attributes.width;
  int windowHeight = attributes.height;
  cv::Mat windowImg = cv::Mat(windowHeight, windowWidth, CV_8UC4);
  int frameCount = 0;
  std::chrono::time_point fpsTime = std::chrono::high_resolution_clock::now();
  unsigned int keycode = XKeysymToKeycode(display, XK_2);
  XTestFakeKeyEvent(display, keycode, True, 0);
  XFlush(display);
  while (true) {
    windowToMat(display, gameWindow, windowWidth, windowHeight, windowImg);
    int progress = getProgress(windowImg);
    frameCount++;
    if (progress >= endProgress) {
      std::cout << "treshold reached, releasing" << std::endl;
      std::cout << "progress: " << progress << std::endl;
      XTestFakeKeyEvent(display, keycode, False, 0);
      XFlush(display);
      std::this_thread::sleep_for(std::chrono::seconds(2));
      std::cout << "waiting time is over, clicking" << std::endl;
      XTestFakeKeyEvent(display, keycode, True, 0);
      XFlush(display);
      fpsTime = std::chrono::high_resolution_clock::now();
      frameCount = 0;
      continue;
    }
    if (keyIsPressed(display, XK_Control_L) &&  keyIsPressed(display, XK_S)) {
      std::cout << "ctrl + s is pressed, stopping" << std::endl;
      XTestFakeKeyEvent(display, keycode, False, 0);
      XFlush(display);
      break;
    }
    std::chrono::time_point now = std::chrono::high_resolution_clock::now();
    long milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - fpsTime).count();
    if (milliseconds > 1000) {
      std::cout << frameCount << std::endl;
      std::cout << "progress: " << progress << std::endl;
      fpsTime = now;
      frameCount = 0;
    }
  }
  XCloseDisplay(display);
  return 0;
}
