//
// Created by egi on 10/7/17.
//

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <cmath>

class MPISound {
public:
    class FunctionCallMeta {
    public:
        enum class FunctionType {
            RECV, SEND
        };

        static char type_to_char(FunctionType type) {
            switch(type) {
                case FunctionType::RECV: return 'r';
                case FunctionType::SEND: return 's';
            }
        }

        FunctionCallMeta(double duration, double start_time, FunctionType type, int rank)
                : m_duration(duration)
                , m_start_time(start_time)
                , m_type(type)
                , m_rank(rank)
        { }

        double m_duration;
        double m_start_time;
        FunctionType m_type;
        int m_rank;
    };

    MPISound() {
        m_size = get_size_from_dir();

        m_scale = 1.0/500000;

        if(m_size < 1) {
            std::cerr << "Error! No output.t files in this directory!\n";
        }
        else {
            read();
        }

        std::cout << " Find " << m_size << " output.t files in this directory!\n";
    }

    int get_size_from_dir() {
        for(int i = 0; i < std::numeric_limits<int>::max(); ++i) {
            auto filename = "rank_" + std::to_string(i) + "_output.t";
            if(!file_exist(filename)) {
                return i;
            }
        }
    }

    bool file_exist(std::string& file) {
        std::ifstream infile(file);

        return infile.good();
    }

    void read() {
        for(int rank = 0; rank < m_size; ++rank) {
            read(rank);
        }
    }

    void read(int rank) {
        auto filename = "rank_" + std::to_string(rank) + "_output.t";

        std::ifstream infile(filename);

        std::string line;
        while(std::getline(infile, line)) {
            std::istringstream iss(line);

            double start_time;
            double duration;
            char type;

            if(!(iss >> type >> start_time >> duration)) {
                break;
            }

            FunctionCallMeta::FunctionType type_symbol;

            if(type == 's') {
                type_symbol = FunctionCallMeta::FunctionType::SEND;
            }
            else if(type == 'r') {
                type_symbol = FunctionCallMeta::FunctionType::RECV;
            }

            m_metadata.push_back(FunctionCallMeta(duration * m_scale, start_time * m_scale, type_symbol, rank));

            std::cout << "Meta -> " << type << " -> start at -> " << m_metadata.back().m_start_time << " duration -> " << m_metadata.back().m_duration << "\n";
        }
    }

    double get_max_time() {
        double max_time = 0.0;

        for(auto& meta: m_metadata) {
            if(max_time < meta.m_start_time + meta.m_duration) {
                max_time = (meta.m_start_time + meta.m_duration);
            }
        }

        return max_time;
    }

    int operation_in_time(double time, int rank) {
        // TODO Make some optimization

        for(auto& meta: m_metadata) {
            if(meta.m_rank == rank) {
                if(time > meta.m_start_time && time < meta.m_start_time + meta.m_duration) {
                    if(meta.m_type == FunctionCallMeta::FunctionType::SEND) {
                        return 1;
                    }
                    else {
                        return 2;
                    }
                }
            }
        }

        return 0;
    }

    void write(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);

        // Write the file headers
        file << "RIFF----WAVEfmt ";   // (chunk size to be filled in later)
        write_word(file,     16, 4);  // no extension data
        write_word(file,      1, 2);  // PCM - integer samples
        write_word(file,      2, 2);  // two channels (stereo file)
        write_word(file,  44100, 4);  // samples per second (Hz)
        write_word(file, 176400, 4);  // (Sample Rate * BitsPerSample * Channels) / 8
        write_word(file,      4, 2);  // data block size (size of two integer samples, one for each channel, in bytes)
        write_word(file,     16, 2);  // number of bits per sample (use a multiple of 8)

        // Write the data chunk header
        size_t data_chunk_pos = file.tellp();
        file << "data----";  // (chunk size to be filled in later)

        // Write the audio samples
        constexpr double two_pi = 6.283185307179586476925286766559;
        constexpr double max_amplitude = 32760;  // "volume"

        double hz                    = 44100;    // samples per second

        double frequency_send = 261.626;  // middle C4
        double frequency_recv = 293.66;   // middle D4

        double seconds = get_max_time();         // time

        std::cout << " Sound duration will be equal to " << seconds << "\n";

        int N = hz * seconds;  // total number of samples
        for (int n = 0; n < N; n++) {
            // Let change volume of sound
            double amplitude = max_amplitude;

            double frequency_rank_0 = 0;
            double frequency_rank_1 = 0;

            double op_rank_0 = operation_in_time(n / hz, 0);
            double op_rank_1 = operation_in_time(n / hz, 1);

            if(op_rank_0 == 1) {
                frequency_rank_0 = frequency_send;
            }
            else if(op_rank_0 == 2) {
                frequency_rank_0 = frequency_recv;
            }

            if(op_rank_1 == 1) {
                frequency_rank_1 = frequency_send;
            }
            else if(op_rank_1 == 2) {
                frequency_rank_1 = frequency_recv;
            }

            double rank_0 = std::sin( (two_pi * n * frequency_rank_0) / hz );
            double rank_1 = std::sin( (two_pi * n * frequency_rank_1) / hz );

            write_word(file, (int)(amplitude * rank_0), 2 ); // Left  headphone
            write_word(file, (int)(amplitude * rank_1), 2 ); // Right headphone
        }

        // (We'll need the final file size to fix the chunk sizes above)
        size_t file_length = file.tellp();

        // Fix the data chunk header to contain the data size
        file.seekp(data_chunk_pos + 4);
        write_word(file, file_length - data_chunk_pos + 8);

        // Fix the file header to contain the proper RIFF chunk size, which is (file size - 8) bytes
        file.seekp(0 + 4);
        write_word(file, file_length - 8, 4);
    }

private:
    template <typename Word>
    std::ostream& write_word( std::ostream& outs, Word value, unsigned size = sizeof( Word ) ) {
        for (; size; --size, value >>= 8) {
            outs.put( static_cast <char> (value & 0xFF) );
        }
        return outs;
    }

    std::vector<FunctionCallMeta> m_metadata;

    double m_scale;
    int    m_size;
};

int main() {
    MPISound sound;
    sound.write("out.wav");

    return 0;
}