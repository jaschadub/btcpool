/*
 The MIT License (MIT)

 Copyright (c) [2016] [BTC.COM]

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */
#ifndef JOB_MAKER_H_
#define JOB_MAKER_H_

#include "Common.h"
#include "Kafka.h"
#include "Stratum.h"

#include <uint256.h>
#include <base58.h>

#include "rsk/RskWork.h"

#include <map>
#include <deque>
#include <vector>
#include <memory>
#include <functional>

using std::vector;
using std::shared_ptr;

// Consume a kafka message and decide whether to generate a new job.
// Params:
//     msg: kafka message.
// Return:
//     if true, JobMaker will try generate a new job.
using JobMakerMessageProcessor = std::function<bool(const string &msg)>;

struct JobMakerConsumerHandler {
  string kafkaTopic_;
  shared_ptr<KafkaConsumer> kafkaConsumer_;
  JobMakerMessageProcessor messageProcessor_;
};

struct JobMakerDefinition
{
  virtual ~JobMakerDefinition() {}

  string chainType_;
  bool enabled_;

  string jobTopic_;
  uint32 jobInterval_;

  string zookeeperLockPath_;
  string fileLastJobTime_;
};

struct GwJobMakerDefinition : public JobMakerDefinition
{
  virtual ~GwJobMakerDefinition() {}

  string rawGwTopic_;
  uint32 maxJobDelay_;
};

struct GbtJobMakerDefinition : public JobMakerDefinition
{
  virtual ~GbtJobMakerDefinition() {}

  bool testnet_;
  
  string payoutAddr_;
  string coinbaseInfo_;
  uint32 blockVersion_;
  
  string rawGbtTopic_;
  string auxPowTopic_;
  string rskRawGwTopic_;

  uint32 maxJobDelay_;
  uint32 gbtLifeTime_;
  uint32 emptyGbtLifeTime_;

  uint32 rskNotifyPolicy_;
};

class JobMakerHandler
{
public:
  virtual ~JobMakerHandler() {}

  virtual bool initConsumerHandlers(const string &kafkaBrokers, vector<JobMakerConsumerHandler> &handlers) = 0;
  virtual string makeStratumJobMsg() = 0;

  // read-only definition
  virtual const JobMakerDefinition& def() = 0;
};

class GwJobMakerHandler : public JobMakerHandler
{
public:
  virtual ~GwJobMakerHandler() {}

  virtual void init(const GwJobMakerDefinition &def) { def_ = def; }

  virtual bool initConsumerHandlers(const string &kafkaBrokers, vector<JobMakerConsumerHandler> &handlers);

  //return true if need to produce stratum job
  virtual bool processMsg(const string &msg) = 0;

  // read-only definition
  virtual const JobMakerDefinition& def() { return def_; }

protected:
  GwJobMakerDefinition def_;
};

class JobMakerHandlerEth : public GwJobMakerHandler
{
public:
  virtual bool processMsg(const string &msg);
  virtual string makeStratumJobMsg();
private:
  void clearTimeoutMsg();
  shared_ptr<RskWork> previousRskWork_;
  shared_ptr<RskWork> currentRskWork_;
};

class JobMakerHandlerSia : public GwJobMakerHandler
{
  string target_;
  string header_;
  uint32 time_;
  bool validate(JsonNode &work);
public:
  JobMakerHandlerSia();
  virtual bool processMsg(const string &msg);
  virtual string makeStratumJobMsg();
};


class JobMaker {
protected:
  shared_ptr<JobMakerHandler> handler_;
  atomic<bool> running_;

  string kafkaBrokers_;
  KafkaProducer kafkaProducer_;
  
  vector<JobMakerConsumerHandler> kafkaConsumerHandlers_;
  vector<shared_ptr<thread>> kafkaConsumerWorkers_;

  time_t lastJobTime_;
  
protected:
  void consumeKafkaMsg(rd_kafka_message_t *rkmessage, JobMakerConsumerHandler &consumerHandler);

public:
  void produceStratumJob();
  void runThreadKafkaConsume(JobMakerConsumerHandler &consumerHandler);

public:
  JobMaker(shared_ptr<JobMakerHandler> handle, const string& brokers);
  virtual ~JobMaker();

  bool init();
  void stop();
  void run();
};

#endif
