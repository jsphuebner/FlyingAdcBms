#include "include/bmsalgo.h"
#include <iostream>

using namespace std;

int main()
{
    cout << "Testing EstimateSocFromVoltage()" << endl;
    for (float voltage = 3220; voltage < 4300; voltage += 50)
    {
        float soc = BmsAlgo::EstimateSocFromVoltage(voltage);
        cout << "Voltage: " << voltage << " SoC: " << soc << endl;
    }

    cout << endl << "Testing GetChargeCurrent()" << endl;
    for (float soc = -5; soc < 110; soc += 5)
    {
        float cur = BmsAlgo::GetChargeCurrent(soc);
        cout << "SoC: " << soc << " Current: " << cur << endl;
    }
    
    BmsAlgo::SetNominalCapacity(100);
    
    float soc = 50;
    for (int i = 0; i < 100; i++)
    {
        soc = BmsAlgo::CalculateSocFromIntegration(soc, -1000);
        cout << "SoC:" << soc << endl;
    }
    return 0;
}
