#include <jni.h>
#include <string>
#include <vector>
#include <sstream>
#include <cmath>
#include <opencv2/opencv.hpp>

// Estructura para almacenar los datos de entrenamiento
struct TrainingSample {
    int label;
    std::vector<float> descriptor;
};

std::vector<TrainingSample> trainingData;

// ====================================================================
// FUNCIÓN 1: Inicializar el clasificador con los datos de entrenamiento
// ====================================================================
extern "C" JNIEXPORT void JNICALL
Java_com_example_momentos_1hu_1zernike_MainActivity_initClassifier(
        JNIEnv* env,
        jobject /* this */,
        jstring descriptorsData) {
        
    const char *dataChars = env->GetStringUTFChars(descriptorsData, 0);
    std::string dataStr(dataChars);
    env->ReleaseStringUTFChars(descriptorsData, dataChars);

    std::istringstream stream(dataStr);
    std::string line;
    
    // Leer primera línea (header)
    if (std::getline(stream, line)) {
        // Podríamos leer la cantidad, pero no es estrictamente necesario
    }

    trainingData.clear();

    // Leer cada línea
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        std::istringstream lineStream(line);
        
        TrainingSample sample;
        lineStream >> sample.label;
        
        float val;
        while (lineStream >> val) {
            sample.descriptor.push_back(val);
        }
        
        if (!sample.descriptor.empty()) {
            trainingData.push_back(sample);
        }
    }
}

// ====================================================================
// FUNCIÓN 2: Clasificar la imagen dibujada
// ====================================================================
extern "C" JNIEXPORT jint JNICALL
Java_com_example_momentos_1hu_1zernike_MainActivity_classifyImage(
        JNIEnv* env,
        jobject /* this */,
        jbyteArray imageData,
        jint width,
        jint height) {

    // 1. Obtener los bytes y convertirlos a un cv::Mat de 4 canales (RGBA)
    jbyte* bufferPtr = env->GetByteArrayElements(imageData, NULL);
    cv::Mat imgRGBA(height, width, CV_8UC4, (unsigned char*)bufferPtr);
    
    // Convertir a escala de grises
    cv::Mat gray;
    cv::cvtColor(imgRGBA, gray, cv::COLOR_RGBA2GRAY);
    
    env->ReleaseByteArrayElements(imageData, bufferPtr, 0);

    // 2. Preprocesamiento (igual que en Python)
    cv::Mat blurred, binary;
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
    
    // Usamos el umbral para detectar trazos negros sobre fondo blanco
    cv::adaptiveThreshold(blurred, binary, 255, 
                          cv::ADAPTIVE_THRESH_GAUSSIAN_C, 
                          cv::THRESH_BINARY_INV, 25, 8);
                          
    // Operaciones morfológicas para limpiar trazos
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel, cv::Point(-1,-1), 2);
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel, cv::Point(-1,-1), 1);

    // 3. Extraer contornos
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

    if (contours.empty()) {
        return -1; // No se dibujó nada
    }

    // Tomar el contorno más grande
    int largestContourIdx = 0;
    double maxArea = 0;
    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            largestContourIdx = i;
        }
    }
    
    std::vector<cv::Point> contour = contours[largestContourIdx];
    if (contour.size() < 20) {
        return -1; // Contorno demasiado pequeño
    }

    // TÉCNICA DE MEJORA: Suavizado Poligonal (approxPolyDP)
    // Reduce el ruido y temblor causado por el dedo humano al dibujar en la pantalla.
    // Esto iguala la calidad del contorno dibujado con la calidad de las imágenes del dataset original.
    std::vector<cv::Point> smoothContour;
    double epsilon = 0.005 * cv::arcLength(contour, true);
    cv::approxPolyDP(contour, smoothContour, epsilon, true);
    
    if (smoothContour.size() < 10) {
        smoothContour = contour; // Fallback si se simplifica demasiado
    }

    // 4. Calcular el Shape Signature usando Coordenadas Complejas (FFT)
    cv::Moments M = cv::moments(smoothContour);
    if (M.m00 == 0) return -1;

    double cx = M.m10 / M.m00;
    double cy = M.m01 / M.m00;

    // Crear la señal compleja s(n) = (x(n) - cx) + j*(y(n) - cy)
    // En OpenCV para dft, usamos una matriz de 2 canales (real, img)
    cv::Mat complexSignal(smoothContour.size(), 1, CV_32FC2);
    for (size_t i = 0; i < smoothContour.size(); i++) {
        complexSignal.at<cv::Vec2f>(i, 0)[0] = (float)(smoothContour[i].x - cx); // Real
        complexSignal.at<cv::Vec2f>(i, 0)[1] = (float)(smoothContour[i].y - cy); // Imag
    }

    // Aplicar FFT 1D
    cv::Mat dftResult;
    cv::dft(complexSignal, dftResult, cv::DFT_COMPLEX_OUTPUT);

    // Calcular magnitud
    std::vector<cv::Mat> planes;
    cv::split(dftResult, planes);
    cv::Mat magnitude;
    cv::magnitude(planes[0], planes[1], magnitude);

    // Extraer las magnitudes en un vector
    std::vector<float> F_mag(smoothContour.size());
    for (size_t i = 0; i < smoothContour.size(); i++) {
        F_mag[i] = magnitude.at<float>(i, 0);
    }

    // Identificar el armónico fundamental |F(1)| o |F(-1)| 
    // (F[-1] es F_mag[smoothContour.size() - 1])
    float f1 = F_mag[1];
    float f_minus1 = F_mag[smoothContour.size() - 1];
    float fundamental = std::max(f1, f_minus1);

    if (fundamental < 1e-6) return -1;

    // Tomar 12 componentes normalizados
    std::vector<float> descriptor;
    int n_components = 12;
    if (f1 >= f_minus1) {
        for (int i = 2; i < 2 + n_components; i++) {
            if (i < F_mag.size()) {
                descriptor.push_back(F_mag[i] / fundamental);
            } else {
                descriptor.push_back(0.0f);
            }
        }
    } else {
        for (int i = 2; i < 2 + n_components; i++) {
            int idx = smoothContour.size() - i;
            if (idx >= 0) {
                descriptor.push_back(F_mag[idx] / fundamental);
            } else {
                descriptor.push_back(0.0f);
            }
        }
    }

    // 5. Clasificar usando la Distancia Euclídea (1-NN)
    if (trainingData.empty()) return -1;

    int bestLabel = -1;
    float minDistance = 1e9;

    for (const auto& sample : trainingData) {
        float distSq = 0;
        for (size_t i = 0; i < descriptor.size(); i++) {
            float diff = descriptor[i] - sample.descriptor[i];
            distSq += diff * diff;
        }
        float dist = std::sqrt(distSq);

        if (dist < minDistance) {
            minDistance = dist;
            bestLabel = sample.label;
        }
    }

    return bestLabel;
}