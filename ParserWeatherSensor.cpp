#include <iostream>
#include <vector>
#include <string>
#include <regex>
#include <curl/curl.h>
#include <mgl2/mgl.h>
#include <mgl2/fltk.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <atomic>

using namespace std;
namespace fs = std::filesystem;

// Global stop flag
std::atomic<bool> globalStopFlag(false);

// Function to write HTTP response data into a string
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

// Function to fetch data from the HTML page
bool fetchData(const string& url, double& temperature, double& pressure, double& humidity) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        cerr << "Error: unable to initialize cURL" << endl;
        return false;
    }

    string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10 seconds timeout

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        cerr << "HTTP Error: " << curl_easy_strerror(res) << endl;
        return false;
    }

    // Extract data from the HTML response using regex
    regex re(R"((\d+\.\d+)\s(deg|Pa|rH))");
    smatch match;
    vector<double> values;

    string::const_iterator searchStart(response.cbegin());
    while (regex_search(searchStart, response.cend(), match, re)) {
        values.push_back(stod(match[1].str()));
        searchStart = match.suffix().first;
    }

    if (values.size() >= 3) {
        temperature = values[0];
        pressure = values[1];
        humidity = values[2];
        return true;
    }

    cerr << "Error: unable to extract data from the response" << endl;
    return false;
}

// Class for managing the real-time plot using MathGL
class RealtimePlot : public mglDraw {
public:
    RealtimePlot(const string& zone, const string& outputDir, int duration) : running(false), pressureMin(0), pressureMax(0), zone(zone), outputDir(outputDir), duration(duration), stopFlag(false) {
        xData.resize(maxPoints, 0);
        tempData.resize(maxPoints, 0);
        pressureData.resize(maxPoints, 0);
        humidityData.resize(maxPoints, 0);
    }

    void SetWnd(mglWnd* w) { wnd = w; }

    // Function to draw the real-time plot
    int Draw(mglGraph* gr) override {
        std::lock_guard<std::mutex> lock(data_mutex);
        if (xData.empty()) return 0;

        mglData xDataMgl(xData.size(), &xData[0]);
        mglData tempDataMgl(tempData.size(), &tempData[0]);
        mglData pressureDataMgl(pressureData.size(), &pressureData[0]);
        mglData humidityDataMgl(humidityData.size(), &humidityData[0]);
        gr->AddLegend("Pressure", "b");
        gr->AddLegend("Temperature", "r");
        gr->AddLegend("Humidity", "g");
        gr->Legend(1, 1, "A");
        gr->SubPlot(1, 3, 0, "_", 0, 0); // First plot: Temperature
        gr->SetRanges(0, duration, 20, 45); // Finer scale for temperature
        gr->Axis();
        gr->Plot(xDataMgl, tempDataMgl, "r-5");

        gr->SubPlot(1, 3, 0, "_", 0, 1.5); // Second plot: Pressure
        gr->SetRanges(0, duration, pressureMin, pressureMax); // Adapted scale for pressure
        gr->Axis();
        gr->Plot(xDataMgl, pressureDataMgl, "b-5");

        gr->SubPlot(1, 3, 0, "", 0, 3); // Third plot: Humidity
        gr->SetRanges(0, duration, 10, 70); // Finer scale for humidity
        gr->Axis();
        gr->Plot(xDataMgl, humidityDataMgl, "g-5");

        return 0;
    }

    // Function to update the plot data
    void update(double temp, double pressure, double humidity) {
        std::lock_guard<std::mutex> lock(data_mutex);
        if (xData.size() >= maxPoints) {
            xData.erase(xData.begin());
            tempData.erase(tempData.begin());
            pressureData.erase(pressureData.begin());
            humidityData.erase(humidityData.begin());
        }

        xData.push_back(timeIndex++);
        tempData.push_back(temp);
        pressureData.push_back(pressure);
        humidityData.push_back(humidity);

        // Adapt the pressure scale based on the first reading
        if (pressureMin == 0 && pressureMax == 0) {
            pressureMin = pressure - 50; // Adapt the pressure scale
            pressureMax = pressure + 50;
        }

        cout << "Data updated: Temp=" << temp << ", Pressure=" << pressure << ", Humidity=" << humidity << endl;
    }

    // Function to fetch and update data in a loop
    void Calc(const string& url) {
        auto start = std::chrono::steady_clock::now();
        while (running) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
            if (elapsed >= duration || stopFlag || globalStopFlag) {
                break;
            }

            double temp, pressure, humidity;
            if (fetchData(url, temp, pressure, humidity)) {
                update(temp, pressure, humidity);
                if (wnd) wnd->Update();
            }
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Update every 5 seconds
        }
    }

    // Function to start the data fetching and plotting
    void Start(const string& url) {
        running = true;
        calcThread = std::thread(&RealtimePlot::Calc, this, url);
    }

    // Function to stop the data fetching and plotting
    void Stop() {
        running = false;
        stopFlag = true;
        if (calcThread.joinable())
            calcThread.join();
    }

    // Function to generate final graphs after stopping
    void plotFinalGraphs() {
        plotGraph("Temperature", outputDir + "/realtime_plot_temp_" + zone + ".png", tempData, "r", 20, 45); // Finer scale for temperature
        plotGraph("Pressure", outputDir + "/realtime_plot_pressure_" + zone + ".png", pressureData, "b", pressureMin, pressureMax); // Adapted scale for pressure
        plotGraph("Humidity", outputDir + "/realtime_plot_humidity_" + zone + ".png", humidityData, "g", 30, 70); // Finer scale for humidity
    }

    ~RealtimePlot() {
        Stop();
    }

    // Function to write data to a CSV file
    void writeCSV() {
        time_t now = time(0);
        tm* ltm = localtime(&now);
        stringstream ss;
        ss << outputDir << "/data_" << zone << "_" << 1900 + ltm->tm_year << "-" << 1 + ltm->tm_mon << "-" << ltm->tm_mday << "_" << ltm->tm_hour << "-" << ltm->tm_min << "-" << ltm->tm_sec << ".csv";
        ofstream file(ss.str());

        if (file.is_open()) {
            file << "Time,Temperature,Pressure,Humidity\n";
            for (size_t i = 0; i < xData.size(); ++i) {
                file << xData[i] << "," << tempData[i] << "," << pressureData[i] << "," << humidityData[i] << "\n";
            }
            file.close();
            cout << "CSV file created: " << ss.str() << endl;
        }
        else {
            cerr << "Error: unable to create CSV file" << endl;
        }
    }

private:
    // Function to plot and save a graph to a file
    void plotGraph(const string& title, const string& filename, const vector<double>& data, const char* color, double yMin, double yMax) {
        mglGraph gr;
        gr.SetRanges(0, duration, yMin, yMax);
        gr.SetOrigin(0, 0);
        gr.SetQuality(2);
        gr.SetSize(800, 600);
        gr.SetFontSize(2);
        gr.SetFontDef("Arial");
        gr.Axis();
        gr.Label('x', "Time", 0, "k");
        gr.Label('y', title.c_str(), 0, "k");

        mglData xDataMgl(xData.size(), &xData[0]);
        mglData dataMgl(data.size(), &data[0]);

        gr.Plot(xDataMgl, dataMgl, color);
        gr.AddLegend(title.c_str(), color);
        gr.Legend();
        gr.Box();
        gr.WriteFrame(filename.c_str()); // Save the plot to a file
    }

    vector<double> xData, tempData, pressureData, humidityData;
    int maxPoints = 300; // Increased to 500 requests
    int timeIndex = 0;
    double pressureMin, pressureMax;
    std::mutex data_mutex;
    mglWnd* wnd;
    std::thread calcThread;
    bool running;
    string zone;
    string outputDir;
    int duration; // Duration in seconds
    std::atomic<bool> stopFlag; // Flag to stop the measurement
};

// Function to validate IP address format
bool isValidIPAddress(const string& ipAddress) {
    const regex pattern(
        R"((^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$))");
    return regex_match(ipAddress, pattern);
}

int main(int argc, char** argv) {
    string ipAddress;
    do {
        cout << "Enter the IP address: ";
        cin >> ipAddress;

        if (!isValidIPAddress(ipAddress)) {
            cerr << "Invalid IP address format. Please try again." << endl;
        }
    } while (!isValidIPAddress(ipAddress));

    string url = "http://" + ipAddress;

    string zone;
    cout << "Enter the zone of the panel: ";
    cin >> zone;

    int duration;
    cout << "Enter the duration of the measurement in seconds: ";
    cin >> duration;

    // Get the user's desktop path  
    string homeDir = getenv("USERPROFILE");
    string desktopDir = homeDir + "\\Desktop";
    string analysisDir = desktopDir + "\\analisi ventole";
    string zoneDir = analysisDir + "\\" + zone;

    // Create directories if they do not exist  
    try {
        if (!fs::exists(analysisDir)) {
            fs::create_directory(analysisDir);
        }
        if (!fs::exists(zoneDir)) {
            fs::create_directory(zoneDir);
        }
    }
    catch (const fs::filesystem_error& e) {
        cerr << "Error creating directories: " << e.what() << endl;
        return 1;
    }

    RealtimePlot plotter(zone, zoneDir, duration);
    mglFLTK gr(&plotter, "Real-time Plotting");
    plotter.SetWnd(&gr);
    plotter.Start(url);

    // Thread to monitor user input for stopping the measurement
    std::thread inputThread([&plotter]() {
        cout << "Press 'q' and Enter to stop the measurement at any time." << endl;
        char c;
        do {
            cin >> c;
        } while (c != 'q');
        plotter.Stop();
        globalStopFlag = true;
        std::exit(0);
        });

    gr.Run();
    plotter.Stop();
    if (inputThread.joinable())
        inputThread.join();
    plotter.plotFinalGraphs(); // Generate final graphs after stopping  
    plotter.writeCSV(); // Generate CSV file after stopping
    return 0;
}
