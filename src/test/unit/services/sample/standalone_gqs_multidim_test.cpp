#include <stan/services/error_codes.hpp>
#include <stan/services/sample/standalone_gqs.hpp>
#include <stan/callbacks/stream_writer.hpp>
#include <stan/callbacks/stream_logger.hpp>
#include <stan/io/dump.hpp>
#include <stan/io/stan_csv_reader.hpp>
#include <test/test-models/good/services/gq_test_multidim.hpp>
#include <test/unit/services/instrumented_callbacks.hpp>
#include <test/unit/util.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <boost/tokenizer.hpp>

class ServicesStandaloneGQ2 : public ::testing::Test {
public:
  ServicesStandaloneGQ2() :
    logger(logger_ss, logger_ss, logger_ss, logger_ss, logger_ss) {}

 void SetUp() {
  std::fstream data_stream("src/test/test-models/good/services/gq_test_multidim.data.R",
                           std::fstream::in);
  stan::io::dump data_var_context(data_stream);
  data_stream.close();
  model = new stan_model(data_var_context);
 }

  void TearDown() {
    delete(model);
  }

  stan::test::unit::instrumented_interrupt interrupt;
  std::stringstream logger_ss;
  stan::callbacks::stream_logger logger;
  stan_model *model;
};

typedef boost::tokenizer<boost::char_separator<char>> tokenizer;

TEST_F(ServicesStandaloneGQ2, genDraws_gq_test_multidim) {
  stan::io::stan_csv multidim_csv;
  std::stringstream out;
  std::ifstream csv_stream;
  csv_stream.open("src/test/test-models/good/services/gq_test_multidim_fit.csv");
  multidim_csv = stan::io::stan_csv_reader::parse(csv_stream, &out);
  csv_stream.close();
  EXPECT_EQ(12345U, multidim_csv.metadata.seed);
  ASSERT_EQ(247, multidim_csv.header.size());
  EXPECT_EQ("p_ar_mat[1,1,1,1]", multidim_csv.header(7));
  EXPECT_EQ("p_ar_mat[4,5,2,3]", multidim_csv.header(126));
  EXPECT_EQ("gq_ar_mat[1,1,1,1]", multidim_csv.header(127));
  EXPECT_EQ("gq_ar_mat[4,5,2,3]", multidim_csv.header(246));
  ASSERT_EQ(1000, multidim_csv.samples.rows());
  ASSERT_EQ(247, multidim_csv.samples.cols());

  // model gq_test_multidim has 1 param, length 120
  std::vector<std::string> param_names;
  std::vector<std::vector<size_t>> param_dimss;
  stan::services::get_model_parameters(*model, param_names, param_dimss);

  EXPECT_EQ(param_names.size(), 1);
  EXPECT_EQ(param_dimss.size(), 1);
  int total = 1;
  for (size_t i = 0; i < param_dimss[0].size(); ++i) {
    total *= param_dimss[0][i];
  }
  EXPECT_EQ(total, 120);

  std::stringstream sample_ss;
  stan::callbacks::stream_writer sample_writer(sample_ss, "");
  int return_code = stan::services::standalone_generate(*model,
                                    multidim_csv.samples.middleCols<120>(7),
                                    12345,
                                    interrupt,
                                    logger,
                                    sample_writer);
  if (return_code != stan::services::error_codes::OK)
    std::cout << "ERROR: " << logger_ss.str() << std::endl;

  EXPECT_EQ(return_code, stan::services::error_codes::OK);
  EXPECT_EQ(count_matches("gq_ar_mat",sample_ss.str()),120);
  EXPECT_EQ(count_matches("\n",sample_ss.str()),1001);

  // compare standalone to sampler QoIs
  std::stringstream sampler_qoi_ss;
  boost::char_separator<char> newline{"\n"};
  boost::char_separator<char> comma{","};
  tokenizer linetok{sample_ss.str(), newline};
  size_t row = 0;
  for (const auto &line : linetok) {
    if (row == 0) {
      ++row;
      continue;
    } 
    tokenizer numtok{line, comma};
    for (const auto &qoi : numtok) {
      sampler_qoi_ss.str(std::string());
      sampler_qoi_ss.clear();
      sampler_qoi_ss << multidim_csv.samples(row-1,127);  // 7 diagnostics, 120 params
      EXPECT_EQ(qoi, sampler_qoi_ss.str());
      break;
    }
    ++row;
  }

}