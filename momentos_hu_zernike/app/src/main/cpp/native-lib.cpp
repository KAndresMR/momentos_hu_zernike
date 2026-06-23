#include <jni.h>
#include <string>
#include <vector>
#include <sstream>
#include <cmath>
#include <iomanip>
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
// FUNCIÓN AUXILIAR: Remuestrear un contorno a exactamente N puntos
// Esto es crítico para que la FFT genere descriptores comparables
// entre el dataset (Python) y el dibujo manual (C++).
// ====================================================================
std::vector<cv::Point2f> resampleContour(const std::vector<cv::Point>& contour, int N) {
    // Calcular la longitud total del perímetro
    double totalLength = 0;
    std::vector<double> segLengths(contour.size());
    for (size_t i = 0; i < contour.size(); i++) {
        int next = (i + 1) % contour.size();
        double dx = contour[next].x - contour[i].x;
        double dy = contour[next].y - contour[i].y;
        segLengths[i] = std::sqrt(dx * dx + dy * dy);
        totalLength += segLengths[i];
    }

    double step = totalLength / N;
    std::vector<cv::Point2f> resampled;
    resampled.push_back(cv::Point2f((float)contour[0].x, (float)contour[0].y));

    double accumulated = 0;
    double target = step;
    size_t idx = 0;
    double segProgress = 0; // cuánto hemos avanzado en el segmento actual

    while ((int)resampled.size() < N && idx < contour.size()) {
        double remaining = segLengths[idx] - segProgress;
        double needed = target - accumulated;

        if (remaining >= needed) {
            // El punto cae en este segmento
            double t = (segProgress + needed) / segLengths[idx];
            int next = (idx + 1) % contour.size();
            float px = (float)(contour[idx].x + t * (contour[next].x - contour[idx].x));
            float py = (float)(contour[idx].y + t * (contour[next].y - contour[idx].y));
            resampled.push_back(cv::Point2f(px, py));
            segProgress += needed;
            accumulated = 0;
            target = step;
        } else {
            // Avanzar al siguiente segmento
            accumulated += remaining;
            segProgress = 0;
            idx++;
        }
    }

    // Asegurar exactamente N puntos
    while ((int)resampled.size() < N) {
        resampled.push_back(resampled.back());
    }

    return resampled;
}

// ====================================================================
// FUNCIÓN 2: Clasificar la imagen dibujada
// ====================================================================
extern "C" JNIEXPORT jstring JNICALL
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

    // 2. Redimensionar a 256x256 (MISMO tamaño que el preprocesamiento en Python)
    cv::Mat resized;
    cv::resize(gray, resized, cv::Size(256, 256));

    // 3. Preprocesamiento (idéntico al notebook de Python)
    cv::Mat blurred, binary;
    cv::GaussianBlur(resized, blurred, cv::Size(5, 5), 0);
    
    // Binarización con umbral adaptativo
    cv::adaptiveThreshold(blurred, binary, 255, 
                          cv::ADAPTIVE_THRESH_GAUSSIAN_C, 
                          cv::THRESH_BINARY_INV, 25, 8);
                          
    // Operaciones morfológicas para limpiar y RELLENAR la silueta
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel, cv::Point(-1,-1), 2);
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel, cv::Point(-1,-1), 1);

    // RELLENAR la silueta dibujada:
    // El usuario dibuja un contorno con trazo grueso. Necesitamos rellenar el interior
    // para que findContours extraiga la silueta sólida (como en las fotos del dataset).
    // Usamos floodFill desde las esquinas para detectar el fondo, luego invertimos.
    cv::Mat filled = binary.clone();
    cv::Mat mask = cv::Mat::zeros(258, 258, CV_8UC1); // mask debe ser 2px más grande
    
    // Inundar desde las 4 esquinas (el fondo)
    cv::floodFill(filled, mask, cv::Point(0, 0), cv::Scalar(255));
    cv::floodFill(filled, mask, cv::Point(255, 0), cv::Scalar(255));
    cv::floodFill(filled, mask, cv::Point(0, 255), cv::Scalar(255));
    cv::floodFill(filled, mask, cv::Point(255, 255), cv::Scalar(255));
    
    // Invertir: lo que NO es fondo = hoja rellena
    cv::Mat filledInv;
    cv::bitwise_not(filled, filledInv);
    
    // Combinar con la imagen binaria original (unión)
    cv::Mat result;
    cv::bitwise_or(binary, filledInv, result);

    // 4. Extraer contornos de la silueta rellena
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(result, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

    if (contours.empty()) {
        return env->NewStringUTF("-1"); // No se dibujó nada
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
        return env->NewStringUTF("-1"); // Contorno demasiado pequeño
    }

    // 5. REMUESTREAR el contorno a exactamente 128 puntos
    // Esto es CRÍTICO: la FFT debe operar sobre la misma cantidad de puntos
    // que en el notebook de Python para que los descriptores sean comparables.
    const int N_POINTS = 128;
    std::vector<cv::Point2f> resampled = resampleContour(contour, N_POINTS);

    // 6. Calcular el Shape Signature usando Coordenadas Complejas (FFT)
    // Calcular centroide del contorno remuestreado
    float cx = 0, cy = 0;
    for (const auto& pt : resampled) {
        cx += pt.x;
        cy += pt.y;
    }
    cx /= N_POINTS;
    cy /= N_POINTS;

    // Crear la señal compleja s(n) = (x(n) - cx) + j*(y(n) - cy)
    cv::Mat complexSignal(N_POINTS, 1, CV_32FC2);
    for (int i = 0; i < N_POINTS; i++) {
        complexSignal.at<cv::Vec2f>(i, 0)[0] = resampled[i].x - cx; // Real
        complexSignal.at<cv::Vec2f>(i, 0)[1] = resampled[i].y - cy; // Imag
    }

    // Aplicar FFT 1D
    cv::Mat dftResult;
    cv::dft(complexSignal, dftResult, cv::DFT_COMPLEX_OUTPUT);

    // Calcular magnitud (descartar fase = invarianza a rotación)
    std::vector<cv::Mat> planes;
    cv::split(dftResult, planes);
    cv::Mat magnitude;
    cv::magnitude(planes[0], planes[1], magnitude);

    // Extraer las magnitudes en un vector
    std::vector<float> F_mag(N_POINTS);
    for (int i = 0; i < N_POINTS; i++) {
        F_mag[i] = magnitude.at<float>(i, 0);
    }

    // Normalizar por el primer armónico |F(1)| para invarianza a escala
    float f1 = F_mag[1];
    float f_minus1 = F_mag[N_POINTS - 1];
    float fundamental = std::max(f1, f_minus1);

    if (fundamental < 1e-6) return env->NewStringUTF("-1");

    // Tomar 12 componentes normalizados (descartando DC y fundamental)
    std::vector<float> descriptor;
    int n_components = 12;
    if (f1 >= f_minus1) {
        for (int i = 2; i < 2 + n_components; i++) {
            if (i < N_POINTS) {
                descriptor.push_back(F_mag[i] / fundamental);
            } else {
                descriptor.push_back(0.0f);
            }
        }
    } else {
        for (int i = 2; i < 2 + n_components; i++) {
            int idx = N_POINTS - i;
            if (idx >= 0) {
                descriptor.push_back(F_mag[idx] / fundamental);
            } else {
                descriptor.push_back(0.0f);
            }
        }
    }

    // 7. Clasificar usando la Distancia Euclídea (1-NN)
    if (trainingData.empty()) return env->NewStringUTF("-1");

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

    // Construir la cadena de resultado: "clase|desc1, desc2, desc3, desc4, ..."
    std::ostringstream resultStr;
    if (bestLabel == -1) {
        resultStr << "-1";
    } else {
        resultStr << bestLabel << "|";
        for (int i = 0; i < std::min(4, (int)descriptor.size()); i++) {
            resultStr << std::fixed << std::setprecision(4) << descriptor[i];
            if (i < 3) resultStr << ", ";
        }
        resultStr << ", ...";
    }

    return env->NewStringUTF(resultStr.str().c_str());
}