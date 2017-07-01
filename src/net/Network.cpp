/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2016-2017 XMRig       <support@xmrig.com>
 *
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <memory>


#include "log/Log.h"
#include "net/Client.h"
#include "net/Network.h"
#include "net/strategies/DonateStrategy.h"
#include "net/strategies/FailoverStrategy.h"
#include "net/strategies/SinglePoolStrategy.h"
#include "net/Url.h"
#include "Options.h"
#include "workers/Workers.h"


Network::Network(const Options *options) :
    m_donateActive(false),
    m_options(options),
    m_donate(nullptr)
{
    Workers::setListener(this);
    m_agent = userAgent();

    const std::vector<Url*> &pools = options->pools();

    if (pools.size() > 1) {
        m_strategy = new FailoverStrategy(pools, m_agent, this);
    }
    else {
        m_strategy = new SinglePoolStrategy(pools.front(), m_agent, this);
    }

    if (m_options->donateLevel() > 0) {
        m_donate = new DonateStrategy(m_agent, this);
    }
}


Network::~Network()
{
    free(m_agent);
}


void Network::connect()
{
    m_strategy->connect();
}


void Network::onActive(Client *client)
{
    if (client->id() == -1) {
        LOG_NOTICE("dev donate started");
        return;
    }

    LOG_INFO(m_options->colors() ? "\x1B[01;37muse pool: \x1B[01;36m%s:%d" : "use pool: %s:%d", client->host(), client->port());
}


void Network::onJob(Client *client, const Job &job)
{
    if (m_donate && m_donate->isActive() && client->id() != -1) {
        return;
    }

    setJob(client, job);
}


void Network::onJobResult(const JobResult &result)
{
    LOG_NOTICE(m_options->colors() ? "\x1B[01;32mSHARE FOUND" : "SHARE FOUND");

    if (result.poolId == -1 && m_donate) {
        return m_donate->submit(result);
    }

    m_strategy->submit(result);
}


void Network::onPause(IStrategy *strategy)
{
    if (m_donate && m_donate == strategy) {
        LOG_NOTICE("dev donate finished");
        m_strategy->resume();
    }

    if (!m_strategy->isActive()) {
        LOG_ERR("no active pools, pause mining");
        return Workers::pause();
    }
}


void Network::setJob(Client *client, const Job &job)
{
    if (m_options->colors()) {
        LOG_INFO("\x1B[01;35mnew job\x1B[0m from \x1B[01;37m%s:%d\x1B[0m diff: \x1B[01;37m%d", client->host(), client->port(), job.diff());

    }
    else {
        LOG_INFO("new job from %s:%d diff: %d", client->host(), client->port(), job.diff());
    }

    Workers::setJob(job);
}