// #pragma once

#include "read_sample.h"

// g++ -std=c++11 -I/usr/local/include -L/usr/local/lib read_sample.cpp -luhd  -o read_sample

// Assuming the file is a binary file containing complex<float> data
std::vector<std::complex<float>> read_from_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    std::vector<std::complex<float>> buffer;

    if (file.is_open()) {
        std::complex<float> value;
        while (file.read(reinterpret_cast<char*>(&value), sizeof(std::complex<float>))) {
            buffer.push_back(value);
        }
        file.close();
    }

    return buffer;
}

int main(){
    std::vector<std::complex<float>> dl_buffer = read_from_file("/home/anonymous/signal_data/dl_samples_0.bin");
    std::vector<std::complex<float>> ul_buffer = read_from_file("/home/anonymous/signal_data/ul_samples_0.bin");

    int srsran_rf_recv_wrapper( void* h,
                            cf_t* data_[SRSRAN_MAX_PORTS], 
                            uint32_t nsamples, 
                            srsran_timestamp_t* t);

    // initial configuration
    cell_search_cfg_t cell_detect_config = {.max_frames_pbch    = SRSRAN_DEFAULT_MAX_FRAMES_PBCH,
                                        .max_frames_pss       = SRSRAN_DEFAULT_MAX_FRAMES_PSS,
                                        .nof_valid_pss_frames = SRSRAN_DEFAULT_NOF_VALID_PSS_FRAMES,
                                        .init_agc             = 0,
                                        .force_tdd            = false};
    srsran_cell_t      cell;
    falcon_ue_dl_t     falcon_ue_dl;
    srsran_dl_sf_cfg_t dl_sf;
    srsran_pdsch_cfg_t pdsch_cfg;
    srsran_ue_sync_t   ue_sync;
    srsran_rf_t* rf;
    
    // Cell Config
    cell.nof_prb          = 50;
    cell.id               = 1;
    cell.nof_ports        = 2;
    cell.cp               = SRSRAN_CP_NORM;
    cell.phich_length     = SRSRAN_PHICH_NORM;
    cell.phich_resources  = SRSRAN_PHICH_R_1_6;

    // Args Config

    uint32_t nof_subframes = DEFAULT_NOF_SUBFRAMES_TO_CAPTURE;
    int cpu_affinity = -1;
    bool enable_ASCII_PRB_plot = true;
    bool enable_ASCII_power_plot = false;
    bool disable_cfo = false;
    uint32_t time_offset = 0;
    int force_N_id_2 = -1; // Pick the best
    int file_offset_time = 0;
    double file_offset_freq = 0;
    uint32_t nof_prb = DEFAULT_NOF_PRB;
    uint32_t file_nof_prb = DEFAULT_NOF_PRB;
    uint32_t file_nof_ports = DEFAULT_NOF_PORTS;
    uint32_t file_cell_id = 0;
    bool file_wrap = false;

    double rf_freq = 2680e6;
    double ul_freq = 2560e6;
    uint32_t rf_nof_rx_ant = DEFAULT_NOF_RX_ANT;
    int decimate = 0;
    int nof_sniffer_thread = DEFAULT_NOF_THREAD;
    // other args
    uint32_t dci_format_split_update_interval_ms = DEFAULT_DCI_FORMAT_SPLIT_UPDATE_INTERVAL_MS;
    double dci_format_split_ratio = DEFAULT_DCI_FORMAT_SPLIT_RATIO;
    bool skip_secondary_meta_formats = false;
    bool enable_shortcut_discovery = true;
    uint32_t rnti_histogram_threshold = DEFAULT_RNTI_HISTOGRAM_THRESHOLD;
    std::string pcap_file = "ul_sniffer.pcap";
    int harq_mode = 0;
    uint16_t rnti = SRSRAN_SIRNTI;
    int mcs_tracking_mode = 1;
    // char* rf_dev = "";
    int verbose = 0;
    int enable_cfo_ref = 1;
    std::string estimator_alg = "interpolate";
    bool cell_search = false;
    uint32_t cell_id = 0;
    int sniffer_mode = 1;
    uint16_t target_rnti = 0;
    bool en_debug = false;
    int api_mode = -1; //api functions, 0: identity mapping, 1: UECapa, 2: IMSI, 3: all functions

    // rf_buffer_interface* buffer;
    srsran::rf_buffer_interface* buf;
    // buf->set_nof_samples();
    HARQ harq;
    harq.init_HARQ(harq_mode);
    UL_HARQ  ul_harq;
    ULSchedule  ulsche(target_rnti, &ul_harq, en_debug);
    ulsche.set_multi_offset(sniffer_mode);

    int ret, n;                 // return
    uint8_t mch_table[10];      // unknown
    float search_cell_cfo = 0;  // freg. offset
    uint32_t sfn = 0;           // system frame number
    uint32_t skip_cnt = 0;      // number of skipped subframe
    uint32_t total_sf = 0;
    uint32_t skip_last_1s = 0;
    uint16_t nof_lost_sync = 0;
    int mcs_tracking_timer = 0;
    int update_rnti_timer = 0;

    int srate = srsran_sampling_freq_hz(cell.nof_prb);
    if (srate != -1) {
      printf("Setting sampling rate %.2f MHz\n", (float)srate / 1000000);
    //   float srate_rf = srsran_rf_set_rx_srate(&rf, (double)srate);
    }

    std::string input_file_name = "/home/anonymous/signal_data/1845.bin.sigmf-data";

    char* tmp_filename = new char[input_file_name.length()+1];
    strncpy(tmp_filename, input_file_name.c_str(), input_file_name.length());
    tmp_filename[input_file_name.length()] = 0;
    if (srsran_ue_sync_init_file_multi(&ue_sync,
                                       cell.nof_prb,
                                       tmp_filename,
                                       file_offset_time,
                                       file_offset_freq,
                                       rf_nof_rx_ant)) { //args.rf_nof_rx_ant
      ERROR("Error initiating ue_sync");
      exit(-1);
    }

    delete[] tmp_filename;
    tmp_filename = nullptr;

    if (srsran_ue_sync_set_cell(&ue_sync, cell)) {
      ERROR("Error initiating ue_sync");
      exit(-1);
    }
    // while (!go_exit && (sf_cnt < args.nof_subframes || args.nof_subframes == 0)){

    // /* Set default verbose level */
    // set_srsran_verbose_level(args.verbose);
    // ret = srsran_ue_sync_zerocopy(&ue_sync, cur_worker->getBuffers(), max_num_samples);
    // if (ret < 0) {
    //   if (args.input_file_name != ""){
    //     std::cout << "Finish reading from file" << std::endl;
    //   }
    //   ERROR("Error calling srsran_ue_sync_work()");
    // }

    
    // srsran::console("Opening channels idx %d in RF device abstraction\n", &rf);

    // if (srsran_rf_open_file(rf_device, rx_files, tx_files, nof_channels, base_srate)) {
    //     logger.error("Error opening RF device abstraction");
    //     return false;
    // }
}