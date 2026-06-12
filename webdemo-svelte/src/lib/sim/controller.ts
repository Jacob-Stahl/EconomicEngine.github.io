import type {
  ABM,
  ClassHandle,
  ConsumerManager,
  Depth,
  MainModule,
  Observation,
  ProducerManager,
  Spread,
  VectorPriceBin,
} from '../generated/eelib';
import { loadEelib } from '../wasm/eelib';
import type {
  DepthBinSnapshot,
  DepthSnapshot,
  ObservationSnapshot,
  SimulationParams,
  SimulationStatus,
  SpreadSnapshot,
} from './types';

export type SimulationHandlers = {
  onObservation: (snapshot: ObservationSnapshot) => void;
  onStatus: (status: SimulationStatus) => void;
  onError?: (error: unknown) => void;
};

function safeDelete(handle: Partial<ClassHandle> | null | undefined) {
  if (!handle || typeof handle.delete !== 'function') {
    return;
  }

  try {
    handle.delete();
  } catch {
    // Ignore repeated or failed cleanup during boilerplate teardown.
  }
}

function readBins(bins: VectorPriceBin): DepthBinSnapshot[] {
  const snapshots: DepthBinSnapshot[] = [];

  for (let index = 0; index < bins.size(); index += 1) {
    const bin = bins.get(index);
    if (!bin) {
      continue;
    }

    snapshots.push({
      price: bin.price,
      totalQty: bin.totalQty,
    });
  }

  return snapshots;
}

function toSpreadSnapshot(spread: Spread | undefined): SpreadSnapshot | null {
  if (!spread) {
    return null;
  }

  return {
    bidsMissing: spread.bidsMissing,
    asksMissing: spread.asksMissing,
    highestBid: spread.highestBid,
    lowestAsk: spread.lowestAsk,
  };
}

function toDepthSnapshot(depth: Depth | undefined): DepthSnapshot | null {
  if (!depth) {
    return null;
  }

  try {
    return {
      bidBins: readBins(depth.bidBins),
      askBins: readBins(depth.askBins),
    };
  } finally {
    safeDelete(depth.bidBins);
    safeDelete(depth.askBins);
  }
}

export class SimulationController {
  private readonly asset = 'FOOD';
  private readonly tickDelayMs = 1;
  private module: MainModule | null = null;
  private abm: ABM | null = null;
  private consumerManager: ConsumerManager | null = null;
  private producerManager: ProducerManager | null = null;
  private timerId: number | null = null;

  async ready(): Promise<MainModule> {
    if (!this.module) {
      this.module = await loadEelib();
    }

    return this.module;
  }

  async start(params: SimulationParams, handlers: SimulationHandlers): Promise<void> {
    const module = await this.ready();

    handlers.onStatus({
      phase: 'loading',
      message: 'Initializing simulation…',
    });

    this.stopLoop();
    this.destroySimulation();
    this.createSimulation(module, params);

    handlers.onStatus({
      phase: 'running',
      message: 'Simulation is running.',
    });

    this.tick(handlers);
  }

  updateParams(params: SimulationParams) {
    if (!this.consumerManager || !this.producerManager) {
      return;
    }

    this.consumerManager.changeHungerDelay(BigInt(params.hungerDelayMean), BigInt(params.hungerDelayStd));
    this.consumerManager.changeMaxPrice(params.maxPrice, 0);
    this.consumerManager.changeNumAgents(params.numConsumers);
    this.producerManager.changePreferedPrice(params.producerPrice, 0);
    this.producerManager.changeNumAgents(1);
  }

  destroy() {
    this.stopLoop();
    this.destroySimulation();
  }

  private createSimulation(module: MainModule, params: SimulationParams) {
    const abm = new module.ABM();
    const consumerManager = module.createConsumerManager(abm, 'consumers', this.asset);
    const producerManager = module.createProducerManager(abm, 'producer', this.asset);

    if (!consumerManager || !producerManager) {
      safeDelete(consumerManager);
      safeDelete(producerManager);
      safeDelete(abm);
      throw new Error('Failed to create one or more manager handles.');
    }

    this.abm = abm;
    this.consumerManager = consumerManager;
    this.producerManager = producerManager;
    this.updateParams(params);
  }

  private destroySimulation() {
    safeDelete(this.consumerManager);
    safeDelete(this.producerManager);
    safeDelete(this.abm);

    this.consumerManager = null;
    this.producerManager = null;
    this.abm = null;
  }

  private stopLoop() {
    if (this.timerId !== null) {
      window.clearTimeout(this.timerId);
      this.timerId = null;
    }
  }

  private tick(handlers: SimulationHandlers) {
    if (!this.abm) {
      return;
    }

    try {
      this.abm.simStep();
      handlers.onObservation(this.snapshotObservation(this.abm.getLatestObservation()));
      this.timerId = window.setTimeout(() => this.tick(handlers), this.tickDelayMs);
    } catch (error) {
      this.stopLoop();
      handlers.onStatus({
        phase: 'error',
        message: 'Simulation stopped after a runtime error.',
      });
      handlers.onError?.(error);
    }
  }

  private snapshotObservation(observation: Observation): ObservationSnapshot {
    try {
      const time = Number(observation.time);
      const assetObs = observation.assetObservations.get(this.asset);
      const spread = toSpreadSnapshot(assetObs?.spread);
      const depth = toDepthSnapshot(assetObs?.depth);

      return {
        time,
        spread,
        depth,
      };
    } finally {
      safeDelete(observation.assetObservations);
    }
  }
}