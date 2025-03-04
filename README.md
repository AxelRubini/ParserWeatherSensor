# Weather Sensor Real-time Plotter

![Weather Sensor](https://img.shields.io/badge/Weather-Sensor-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

## Overview

The **Weather Sensor Real-time Plotter** is a C++ application that fetches weather data from a specified IP address and visualizes it in real-time using MathGL. The application captures temperature, pressure, and humidity data, and provides real-time plotting and CSV export functionalities.

## Features

- **Real-time Data Fetching**: Continuously fetches weather data from a specified IP address.
- **Real-time Plotting**: Visualizes temperature, pressure, and humidity data in real-time using MathGL.
- **CSV Export**: Exports the captured data to a CSV file for further analysis.
- **Customizable Zones**: Allows users to specify different zones for data categorization and storage.

## Requirements

- **C++17** or later
- **cURL** library
- **MathGL** library
- **FLTK** library
- **Visual Studio 2022** (or any compatible C++ IDE)

## Installation

1. **Clone the repository**:git clone https://github.com/username/WeatherSensorProject.git cd WeatherSensorProject

2. **Install dependencies**:
   - Ensure that cURL, MathGL, and FLTK libraries are installed and properly configured in your development environment.

3. **Build the project**:
   - Open the project in Visual Studio 2022.
   - Build the solution to compile the application.

## Usage

1. **Run the application**:
   - Execute the compiled binary or run the project from Visual Studio.

2. **Enter the IP address**:
   - When prompted, enter the IP address of the weather sensor.

3. **Enter the zone**:
   - Specify the zone for the data categorization.

4. **Real-time Plotting**:
   - The application will start fetching and plotting the data in real-time.

5. **Export Data**:
   - After stopping the application, the data will be exported to a CSV file and saved in the specified directory.

## Example
Enter the IP address: 192.168.1.100 Enter the zone of the panel: Zone1

## Directory Structure

WeatherSensorProject/ ├── src/ │   └── ParserWeatherSensor.cpp ├── include/ │   └── (header files) ├── build/ │   └── (compiled binaries) ├── data/ │   └── (exported CSV files) └── README.md



   