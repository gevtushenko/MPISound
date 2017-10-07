//
// Created by egi on 10/6/17.
//

#include <fstream>
#include <vector>
#include <memory>
#include <chrono>

#include <dlfcn.h>

#include <mpi.h>

typedef int (*recv_t)(void*, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status*);
typedef int (*send_t)(const void*, int, MPI_Datatype, int, int, MPI_Comm);
typedef int (*init_t)(int*, char***);
typedef int (*finalize_t)();

class MPIShell {
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

        FunctionCallMeta(double duration, double start_time, FunctionType type)
            : m_duration(duration)
            , m_start_time(start_time)
            , m_type(type)
        { }

        double m_duration;
        double m_start_time;
        FunctionType m_type;
    };

    using Ptr = std::shared_ptr<MPIShell>;

    MPIShell()
        : m_path_to_mpi_lib("/usr/lib64/openmpi/lib/libmpi_cxx.so")
        , m_path_to_output("output.t")
    {
        m_handle = dlopen(m_path_to_mpi_lib.c_str(), RTLD_LAZY);

        // Load MPI_Recv
        m_recv_fn = (recv_t) dlsym(m_handle, "MPI_Recv");
        const char *dlsym_error = dlerror();

        if(dlsym_error) {
            std::cerr << "Error! Cannot load symbol 'MPI_Recv': " << dlsym_error << '\n';
            dlclose(m_handle);
        }

        // Load MPI_Send
        m_send_fn = (send_t) dlsym(m_handle, "MPI_Send");

        if(dlsym_error) {
            std::cerr << "Error! Cannot load symbol 'MPI_Send': " << dlsym_error << '\n';
            dlclose(m_handle);
        }

        // Load MPI_Init
        m_init_fn = (init_t) dlsym(m_handle, "MPI_Init");
        dlsym_error = dlerror();

        if(dlsym_error) {
            std::cerr << "Error! Cannot load symbol 'MPI_Init': " << dlsym_error << '\n';
            dlclose(m_handle);
        }

        // Load MPI_Finalyze
        m_finalize_fn = (finalize_t) dlsym(m_handle, "MPI_Finalize");
        dlsym_error = dlerror();

        if(dlsym_error) {
            std::cerr << "Error! Cannot load symbol 'MPI_Finalize': " << dlsym_error << '\n';
            dlclose(m_handle);
        }

        m_start_time = std::chrono::steady_clock::now();
    }

    static Ptr instance() {
        static Ptr shell = std::shared_ptr< MPIShell >(new MPIShell());

        return shell;
    }

    int init(int* argc, char*** argv) {
        if(check_handle()) {
            int answer = m_init_fn(argc, argv);

            // Check out rank
            MPI_Comm_rank(MPI_COMM_WORLD, &m_rank);

            return answer;
        }
    }

    int finalize() {
        if(check_handle()) {
            int anwer = m_finalize_fn();

            // Write to file

            std::ofstream output("rank_" + std::to_string(m_rank) + "_" + m_path_to_output);

            for(auto& meta: m_metainfo) {
                output << FunctionCallMeta::type_to_char(meta.m_type) << " "
                       << meta.m_start_time << " "
                       << meta.m_duration << "\n";
            }

            output.close();
        }
    }

    int recv
        (
                void *buf,
                int count,
                MPI_Datatype datatype,
                int source,
                int tag,
                MPI_Comm comm,
                MPI_Status *status
        ) {
        if(check_handle()) {
            auto begin = std::chrono::steady_clock::now();
                int answer = m_recv_fn(buf, count, datatype, source, tag, comm, status);
            auto end = std::chrono::steady_clock::now();

            double duration   = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
            double start_time = std::chrono::duration_cast<std::chrono::microseconds>(begin - m_start_time).count();

            m_metainfo.push_back(FunctionCallMeta(duration, start_time, FunctionCallMeta::FunctionType::RECV));

                return answer;
        }
    }

    int send
        (
                const void *buf,
                int count,
                MPI_Datatype datatype,
                int dest,
                int tag,
                MPI_Comm comm
        ) {
        if(check_handle()) {
            auto begin = std::chrono::steady_clock::now();
                int answer = m_send_fn(buf, count, datatype, dest, tag, comm);
            auto end = std::chrono::steady_clock::now();

            double duration   = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
            double start_time = std::chrono::duration_cast<std::chrono::microseconds>(begin - m_start_time).count();

            m_metainfo.push_back(FunctionCallMeta(duration, start_time, FunctionCallMeta::FunctionType::SEND));

                return answer;
        }
    }

private:
    bool check_handle() {
        if(!m_handle) {
            std::cerr << "Error! Can't open '" + m_path_to_mpi_lib + "'!\n";
            return false;
        }

        return true;
    }

    std::string m_path_to_mpi_lib;
    std::string m_path_to_output;

    int m_rank;

    std::vector<FunctionCallMeta> m_metainfo;

    std::chrono::steady_clock::time_point m_start_time;

    recv_t m_recv_fn;
    send_t m_send_fn;
    init_t m_init_fn;
    finalize_t m_finalize_fn;

    void*  m_handle;
};

extern "C" {

    int MPI_Init(int *argc, char ***argv) {
        std::cout << " Init intercept! " << std::endl;

        return MPIShell::instance()->init(argc, argv);
    }

    int MPI_Finalize(void) {
        std::cout << " Finalize intercept! " << std::endl;

        return MPIShell::instance()->finalize();
    }

    int MPI_Recv
            (
                    void *buf,
                    int count,
                    MPI_Datatype datatype,
                    int source,
                    int tag,
                    MPI_Comm comm,
                    MPI_Status *status
            )
    {
        std::cout << " Recv intercept! " << std::endl;

        return MPIShell::instance()->recv(buf, count, datatype, source, tag, comm, status);
    }

    int MPI_Send
        (
            const void *buf,
            int count,
            MPI_Datatype datatype,
            int dest,
            int tag, MPI_Comm comm
        )
    {
        std::cout << " Send intercept! " << std::endl;

        return MPIShell::instance()->send(buf, count, datatype, dest, tag, comm);
    }

}
