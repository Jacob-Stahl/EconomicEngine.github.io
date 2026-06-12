<script lang="ts">
  import { onMount } from 'svelte';
  import Controls from './lib/components/Controls.svelte';
  import DepthChart from './lib/components/DepthChart.svelte';
  import SpreadChart from './lib/components/SpreadChart.svelte';
  import { SimulationController, type SimulationHandlers } from './lib/sim/controller';
  import {
    DEFAULT_PARAMS,
    SPREAD_HISTORY_WINDOW,
    type ObservationSnapshot,
    type SimulationParams,
    type SimulationStatus,
    type SpreadHistoryPoint,
  } from './lib/sim/types';

  let controller: SimulationController;
  let params: SimulationParams = { ...DEFAULT_PARAMS };
  let history: SpreadHistoryPoint[] = [];
  let latestObservation: ObservationSnapshot | null = null;
  let tickTimestamps: number[] = [];
  const TPS_WINDOW = 60;
  let status: SimulationStatus = {
    phase: 'loading',
    message: 'Loading WebAssembly module…',
  };
  let errorMessage = '';

  const handlers: SimulationHandlers = {
    onObservation(snapshot) {
      latestObservation = snapshot;
      const now = performance.now();
      tickTimestamps = [...tickTimestamps, now].slice(-TPS_WINDOW);
      history = [
        ...history,
        {
          time: snapshot.time,
          bid: snapshot.spread && !snapshot.spread.bidsMissing ? snapshot.spread.highestBid : null,
          ask: snapshot.spread && !snapshot.spread.asksMissing ? snapshot.spread.lowestAsk : null,
        },
      ].slice(-SPREAD_HISTORY_WINDOW);
    },
    onStatus(nextStatus) {
      status = nextStatus;
      if (nextStatus.phase !== 'error') {
        errorMessage = '';
      }
    },
    onError(error) {
      errorMessage = error instanceof Error ? error.message : String(error);
    },
  };

  async function startSimulation() {
    history = [];
    latestObservation = null;
    tickTimestamps = [];
    errorMessage = '';
    await controller.start(params, handlers);
  }

  function handleParamsChange(nextParams: SimulationParams) {
    params = nextParams;
    controller?.updateParams(nextParams);
  }

  function handleRestart() {
    void startSimulation();
  }

  $: latestTickLabel = latestObservation ? `Tick ${latestObservation.time}` : 'Waiting for first tick';
  $: spreadLabel = latestObservation?.spread
    ? `Bid ${latestObservation.spread.bidsMissing ? 'NA' : latestObservation.spread.highestBid} / Ask ${latestObservation.spread.asksMissing ? 'NA' : latestObservation.spread.lowestAsk}`
    : 'Spread not available yet';
  $: ticksPerSecond = tickTimestamps.length > 1
    ? Math.round((tickTimestamps.length - 1) / ((tickTimestamps[tickTimestamps.length - 1] - tickTimestamps[0]) / 1000))
    : 0;

  onMount(() => {
    controller = new SimulationController();
    let active = true;

    void (async () => {
      try {
        await controller.ready();
        if (!active) {
          return;
        }

        await startSimulation();
      } catch (error) {
        if (!active) {
          return;
        }

        status = {
          phase: 'error',
          message: 'Unable to initialize the WebAssembly module.',
        };
        errorMessage = error instanceof Error ? error.message : String(error);
      }
    })();

    return () => {
      active = false;
      controller.destroy();
    };
  });
</script>

<div class="shell">
  <main class="layout">
    <aside class="sidebar">
      <Controls
        {params}
        disabled={status.phase === 'loading'}
        onParamsChange={handleParamsChange}
        onRestart={handleRestart}
      />

      <section class="panel status-panel">
        <div class="panel-header">
          <p class="panel-eyebrow">Runtime</p>
          <h2>Simulation Status</h2>
        </div>

        <p class="status-message">{status.message}</p>

        {#if errorMessage}
          <p class="error-message">{errorMessage}</p>
        {/if}

        <dl class="metrics">
          <div>
            <dt>Tracked ticks</dt>
            <dd>{history.length}</dd>
          </div>
          <div>
            <dt>Ticks/sec</dt>
            <dd>{ticksPerSecond}</dd>
          </div>
          <div>
            <dt>Consumers</dt>
            <dd>{params.numConsumers}</dd>
          </div>
          <div>
            <dt>Producer price</dt>
            <dd>{params.producerPrice}</dd>
          </div>
        </dl>
      </section>
    </aside>

    <section class="charts">
      <SpreadChart history={history} historyWindow={SPREAD_HISTORY_WINDOW} />
      <DepthChart snapshot={latestObservation} />
    </section>
  </main>
</div>
