#include <iostream>
#include <fstream>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <json/json.h>
#include <set>

/*
 * Task:
    * Железнодорожная сеть задана набором станций. Между некоторыми станциями проложены пути (track). Длина путей задается целыми числами. Маршруты поездов заданы списком станций, через которые они проезжают. Скорости поездов одинаковые (скажем, равны единице). Поезд представляет собой точку.
    * Задача: По заданной конфигурации сети и поездов определить, возникнет ли столкновение.
    * Что требуется показать: Способность к созданию реалистичной и адекватной задаче объектной модели, владение концепциями OOP.
 * Task Clarifications:
    *  Collision:
    *       - Two or more trains on one station in one time
    *       - Two trains cannot have same enter time to one track
    *       - Trains opposite direction on one track with time overlap
    *  Tracks are bidirectional
    *  There can be multiple tracks between same pair of stations (@)
 * Solution Comments:
    * Collision check (both track and station) are performed on each train addition
    * if (@) may not find existing solution, due unordered planning
 */

using Station = std::string;

struct StationRailwayHash
{
    // C++20 unordered feature
    using is_transparent = void;

    size_t operator()(const Station& s) const
    {
        return std::hash<std::string>{}(s);
    }

    size_t operator()(const std::shared_ptr<Station>& s) const
    {
        return std::hash<std::string>{}(*s);
    }
};

struct StationRailwayEq
{
    // C++20 unordered feature
    using is_transparent = void;

    bool operator()(const Station& s1, const Station& s2) const
    {
        return s1 == s2;
    }

    bool operator()(const std::shared_ptr<Station>& s1, const std::shared_ptr<Station>& s2) const
    {
        return *s1 == *s2;
    }

    bool operator()(const std::shared_ptr<Station>& s1, const std::string& s2) const
    {
        return *s1 == s2;
    }

    bool operator()(const std::string& s1, const std::shared_ptr<Station>& s2) const
    {
        return s1 == *s2;
    }
};

struct TrainOnTrackInfo
{
    unsigned int enter;
    bool direction; // forward - true

    bool operator<(const TrainOnTrackInfo& other) const
    {
        return this->enter < other.enter;
    }

    bool operator<(const unsigned int& other) const
    {
        return this->enter < other;
    }
};

struct Track
{
    std::shared_ptr<Station> station1;
    std::shared_ptr<Station> station2;
    unsigned int length;

    friend struct TrackRailwayEq;

    std::set<TrainOnTrackInfo, std::less<>> trains_;

    bool TryAddTrainWithTime(unsigned int new_enter, bool direction)
    {
        unsigned int new_exit = new_enter + length;

        // it->enter >= new_enter
        auto it = trains_.lower_bound(new_enter);
        if (it != trains_.end())
        {
            unsigned int it_exit = it->enter + length;

            // Check if intervals overlap
            if (new_enter <= it_exit && it->enter <= new_exit)
            {
                if (it->direction != direction)
                {
                    // Opposite directions cause collision
                    return false;
                }
                else if (it->enter == new_enter)
                {
                    // Same direction and same enter time cause collision
                    return false;
                }
            }
        }

        if (it != trains_.begin())
        {
            // prev->enter < new_enter
            auto prev = std::prev(it);
            unsigned int prev_exit = prev->enter + length;

            if (new_enter <= prev_exit && prev->enter <= new_exit)
            {
                if (prev->direction != direction)
                {
                    // Opposite directions cause collision
                    return false;
                }
            }
        }

        trains_.insert({new_enter, direction});
        return true;
    }
};

struct TrackRailwayEq
{
    // C++20 unordered feature
    using is_transparent = void;

    bool operator()(const Track& t1, const Track& t2) const
    {
        return t1.station1 == t2.station1 && t1.station2 == t2.station2;
    }

    bool operator()(const std::shared_ptr<Track>& t1, const std::shared_ptr<Track>& t2) const
    {
        return t1->station1 == t2->station1 && t1->station2 == t2->station2;
    }

    bool operator()(const std::shared_ptr<Track>& t1,
                    const std::pair<std::shared_ptr<Station>, std::shared_ptr<Station>>& t2) const
    {
        return t1->station1 == t2.first && t1->station2 == t2.second;
    }

    bool operator()(const std::pair<std::shared_ptr<Station>, std::shared_ptr<Station>>& t2,
                    const std::shared_ptr<Track>& t1) const
    {
        return t1->station1 == t2.first && t1->station2 == t2.second;
    }
};

struct TrackRailwayHash
{
    // C++20 unordered feature
    using is_transparent = void;

    size_t operator()(const Track& trck) const
    {
        return StationRailwayHash{}(trck.station1) ^ StationRailwayHash{}(
            trck.station2);
    }

    size_t operator()(const std::shared_ptr<Track>& trck) const
    {
        return StationRailwayHash{}(trck->station1) ^ StationRailwayHash{}(
            trck->station2);
    }

    size_t operator()(const std::pair<std::shared_ptr<Station>, std::shared_ptr<Station>>& pair) const
    {
        return StationRailwayHash{}(pair.first) ^ StationRailwayHash{}(pair.second);
    }
};

class Train final
{
public:
    Train(const std::vector<std::shared_ptr<Track>>& path,
          const std::vector<std::pair<std::shared_ptr<Station>, unsigned int>>& station_times):
        path_(path)
        , station_times_(station_times)
    {
    }

private:
    std::vector<std::shared_ptr<Track>> path_;
    std::vector<std::pair<std::shared_ptr<Station>, unsigned int>> station_times_;

public:
    const decltype(station_times_)& GetStationTimes() const
    {
        return station_times_;
    }
};

class Railway final
{
public:
    Railway(const Json::Value& net_js)
    {
        for (const Json::Value& station_js : net_js["Stations"])
        {
            std::string station_name = station_js.asString();
            if (!stations_.contains(station_name))
                stations_.insert(std::make_shared<Station>(station_name));
            else
                std::cout << "Duplicate station name: " << station_name << std::endl;
        }

        for (const Json::Value& route_js : net_js["Tracks"])
        {
            auto it1 = stations_.find(route_js["Station1"].asString());
            auto it2 = stations_.find(route_js["Station2"].asString());

            if (it1 != stations_.end() && it2 != stations_.end())
            {
                auto s1 = *it1;
                auto s2 = *it2;
                unsigned int length = route_js["Length"].asUInt();
                Track route{s1, s2, length};
                tracks_.insert(std::make_shared<Track>(route));
            }
            else
            {
                if (it1 == stations_.end())
                {
                    std::cerr << "Error: Station1 not found" << std::endl;
                }
                if (it2 == stations_.end())
                {
                    std::cerr << "Error: Station2 not found" << std::endl;
                }
            }
        }
    }

    void AddTrain(const Json::Value& train_js)
    {
        std::vector<std::shared_ptr<Station>> train_stations;

        for (const Json::Value& station_js : train_js)
        {
            auto it = stations_.find(station_js.asString());
            if (it != stations_.end())
                train_stations.push_back(*it);
            else
            {
                std::cerr << "Error: Station not found" << std::endl;
                throw std::invalid_argument("Station not found");
            }
        }

        std::vector<std::shared_ptr<Track>> train_path;
        std::vector<std::pair<std::shared_ptr<Station>, unsigned int>> station_times;
        unsigned int current_time = 0;

        station_times.emplace_back(train_stations[0], current_time);

        for (int i = 0; i < train_stations.size() - 1; ++i)
        {
            auto s1 = *stations_.find(train_stations[i]);
            auto s2 = *stations_.find(train_stations[i + 1]);

            bool inserted = false;

            // forward direction
            auto possible_tracks = tracks_.equal_range(std::pair{s1, s2});
            auto it = tracks_.end();
            for (it = possible_tracks.first; it != possible_tracks.second; ++it)
            {
                inserted = it->get()->TryAddTrainWithTime(current_time, true);
                if (inserted)
                    break;
            }

            if (!inserted)
            {
                // backward direction
                possible_tracks = tracks_.equal_range(std::pair{s2, s1});
                for (it = possible_tracks.first; it != possible_tracks.second; ++it)
                {
                    inserted = it->get()->TryAddTrainWithTime(current_time, false);
                    if (inserted)
                        break;
                }
            }

            if (!inserted)
            {
                std::cerr << "RailWay has collisions" << std::endl;
                throw std::invalid_argument("RailWay has collisions");
            }

            current_time += it->get()->length;

            station_times.emplace_back(s2, current_time);
            train_path.push_back(*it);
        }

        trains_.emplace_back(std::make_unique<Train>(train_path, station_times));

        if (TrainsHaveStationCollisions())
        {
            std::cerr << "RailWay has collisions" << std::endl;
            throw std::invalid_argument("RailWay has collisions");
        }
    }

    bool TrainsHaveStationCollisions()
    {
        for (auto it = trains_.begin(); it != trains_.end(); ++it)
        {
            for (auto it2 = it + 1; it2 != trains_.end(); ++it2)
            {
                std::unordered_map<std::shared_ptr<Station>, std::unordered_set<unsigned int>> station_times;
                for (const auto& it1arrival : it->get()->GetStationTimes())
                {
                    station_times[it1arrival.first].insert(it1arrival.second);
                }

                for (const auto& it2arrival : it2->get()->GetStationTimes())
                {
                    const std::shared_ptr<Station>& station = it2arrival.first;
                    unsigned int time = it2arrival.second;
                    if (station_times.count(station) && station_times[station].count(time))
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }

private:
    std::vector<std::unique_ptr<Train>> trains_;
    std::unordered_set<std::shared_ptr<Station>, StationRailwayHash, StationRailwayEq> stations_;
    std::unordered_multiset<std::shared_ptr<Track>, TrackRailwayHash, TrackRailwayEq> tracks_;
};

int main()
{
    Json::Value root;
    std::ifstream config_doc("config_doc.json", std::ifstream::binary);
    config_doc >> root;

    for (const Json::Value& test_js : root)
    {
        std::cout << "TEST CASE:\n" << test_js.toStyledString() << std::endl;
        std::shared_ptr<Railway> train_network = std::make_shared<Railway>(test_js["Railway"]);

        bool has_collison = false;
        for (const Json::Value& train_js : test_js["Trains"])
        {
            try
            {
                train_network->AddTrain(train_js);
            }
            catch (std::invalid_argument& e)
            {
                has_collison = true;
            }
        }
        std::cout << "RESULT: " << has_collison << " expected: " << test_js["ExpectedCollision"] <<
            std::endl;
    }

    std::cout << "Railway Has No Collisions" << std::endl;
}
