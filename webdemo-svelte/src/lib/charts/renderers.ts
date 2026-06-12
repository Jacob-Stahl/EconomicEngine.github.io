import type { DepthBinSnapshot, ObservationSnapshot, SpreadHistoryPoint } from '../sim/types';

type MetaElement = HTMLElement | null;

function setMetaText(metaElement: MetaElement, text: string) {
  if (metaElement) {
    metaElement.textContent = text;
  }
}

function drawLine(
  ctx: CanvasRenderingContext2D,
  points: Array<{ index: number; value: number | null }>,
  getX: (index: number) => number,
  getY: (value: number) => number,
  color: string,
) {
  let drawing = false;
  ctx.beginPath();

  points.forEach((point) => {
    const value = point.value;
    if (value === null || value === undefined) {
      drawing = false;
      return;
    }

    const x = getX(point.index);
    const y = getY(value);

    if (!drawing) {
      ctx.moveTo(x, y);
      drawing = true;
    } else {
      ctx.lineTo(x, y);
    }
  });

  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.stroke();

  points.forEach((point) => {
    const value = point.value;
    if (value === null || value === undefined) {
      return;
    }

    const x = getX(point.index);
    const y = getY(value);
    ctx.beginPath();
    ctx.arc(x, y, 3, 0, Math.PI * 2);
    ctx.fillStyle = color;
    ctx.fill();
  });
}

function drawDepthSeries(
  ctx: CanvasRenderingContext2D,
  bins: Array<{ price: number; qty: number }>,
  getX: (price: number) => number,
  getY: (qty: number) => number,
  color: string,
  fillColor: string,
) {
  if (bins.length === 0) {
    return;
  }

  ctx.beginPath();
  ctx.moveTo(getX(bins[0].price), getY(0));

  bins.forEach((bin) => {
    ctx.lineTo(getX(bin.price), getY(bin.qty));
  });

  ctx.lineTo(getX(bins[bins.length - 1].price), getY(0));
  ctx.closePath();
  ctx.fillStyle = fillColor;
  ctx.fill();

  ctx.beginPath();
  bins.forEach((bin, index) => {
    const x = getX(bin.price);
    const y = getY(bin.qty);

    if (index === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  });

  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.stroke();

  bins.forEach((bin) => {
    ctx.beginPath();
    ctx.arc(getX(bin.price), getY(bin.qty), 3, 0, Math.PI * 2);
    ctx.fillStyle = color;
    ctx.fill();
  });
}

function normalizeBins(source: DepthBinSnapshot[] | null | undefined) {
  if (!source) {
    return [];
  }

  return source.map((bin) => ({
    price: bin.price,
    qty: bin.totalQty,
  }));
}

function accumulateBins(
  bins: Array<{ price: number; qty: number }>,
  direction: 'ltr' | 'rtl',
): Array<{ price: number; qty: number }> {
  let running = 0;
  if (direction === 'rtl') {
    for (let i = bins.length - 1; i >= 0; i -= 1) {
      running += bins[i].qty;
      bins[i].qty = running;
    }
  } else {
    for (let i = 0; i < bins.length; i += 1) {
      running += bins[i].qty;
      bins[i].qty = running;
    }
  }
  return bins;
}

export function renderSpreadChart({
  canvas,
  metaElement,
  history,
  historyWindow,
}: {
  canvas: HTMLCanvasElement;
  metaElement: MetaElement;
  history: SpreadHistoryPoint[];
  historyWindow: number;
}) {
  const ctx = canvas.getContext('2d');
  if (!ctx) {
    return;
  }

  const width = canvas.width;
  const height = canvas.height;
  const padding = { top: 24, right: 16, bottom: 32, left: 42 };

  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = '#ffffff';
  ctx.fillRect(0, 0, width, height);

  if (history.length === 0) {
    ctx.fillStyle = '#666';
    ctx.font = '14px monospace';
    ctx.textAlign = 'center';
    ctx.fillText('Waiting for ticks...', width / 2, height / 2);
    setMetaText(metaElement, `Tracking latest ${historyWindow} ticks.`);
    return;
  }

  const values = history.flatMap((point) => {
    const series: number[] = [];
    if (point.bid !== null) {
      series.push(point.bid);
    }
    if (point.ask !== null) {
      series.push(point.ask);
    }
    return series;
  });

  const minValue = values.length ? Math.min(...values) : 0;
  const maxValue = values.length ? Math.max(...values) : 1;
  const range = Math.max(1, maxValue - minValue);
  const chartWidth = width - padding.left - padding.right;
  const chartHeight = height - padding.top - padding.bottom;
  const lastIndex = Math.max(1, history.length - 1);

  const getX = (index: number) => padding.left + (chartWidth * index) / lastIndex;
  const getY = (value: number) =>
    padding.top + chartHeight - ((value - minValue) / range) * chartHeight;

  ctx.strokeStyle = '#e2e2e2';
  ctx.lineWidth = 1;

  for (let index = 0; index <= 4; index += 1) {
    const y = padding.top + (chartHeight * index) / 4;
    ctx.beginPath();
    ctx.moveTo(padding.left, y);
    ctx.lineTo(width - padding.right, y);
    ctx.stroke();
  }

  ctx.beginPath();
  ctx.moveTo(padding.left, padding.top);
  ctx.lineTo(padding.left, height - padding.bottom);
  ctx.lineTo(width - padding.right, height - padding.bottom);
  ctx.strokeStyle = '#999';
  ctx.stroke();

  drawLine(
    ctx,
    history.map((point, index) => ({ index, value: point.bid })),
    getX,
    getY,
    '#0f766e',
  );
  drawLine(
    ctx,
    history.map((point, index) => ({ index, value: point.ask })),
    getX,
    getY,
    '#b91c1c',
  );

  ctx.fillStyle = '#333';
  ctx.font = '12px monospace';
  ctx.textAlign = 'left';
  ctx.fillText(`${maxValue}`, 8, padding.top + 4);
  ctx.fillText(`${minValue}`, 8, height - padding.bottom);

  const firstTime = history[0].time;
  const lastTime = history[history.length - 1].time;
  ctx.textAlign = 'left';
  ctx.fillText(`t=${firstTime}`, padding.left, height - 8);
  ctx.textAlign = 'right';
  ctx.fillText(`t=${lastTime}`, width - padding.right, height - 8);

  ctx.textAlign = 'left';
  ctx.fillStyle = '#0f766e';
  ctx.fillText('Bid', padding.left, 14);
  ctx.fillStyle = '#b91c1c';
  ctx.fillText('Ask', padding.left + 48, 14);

  const latest = history[history.length - 1];
  setMetaText(
    metaElement,
    `Tracking latest ${historyWindow} ticks. Latest tick ${latest.time}: bid=${latest.bid ?? 'NA'} ask=${latest.ask ?? 'NA'}`,
  );
}

export function renderDepthChart({
  canvas,
  metaElement,
  snapshot,
}: {
  canvas: HTMLCanvasElement;
  metaElement: MetaElement;
  snapshot: ObservationSnapshot | null;
}) {
  const ctx = canvas.getContext('2d');
  if (!ctx) {
    return;
  }

  const width = canvas.width;
  const height = canvas.height;
  const padding = { top: 24, right: 16, bottom: 32, left: 42 };
  const spread = snapshot?.spread ?? null;
  const depth = snapshot?.depth ?? null;
  const time = snapshot?.time ?? 'NA';

  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = '#ffffff';
  ctx.fillRect(0, 0, width, height);

  const bidBins = accumulateBins(
    normalizeBins(depth?.bidBins).sort((left, right) => left.price - right.price),
    'rtl',
  );
  const askBins = accumulateBins(
    normalizeBins(depth?.askBins).sort((left, right) => left.price - right.price),
    'ltr',
  );

  if (bidBins.length === 0 && askBins.length === 0) {
    ctx.fillStyle = '#666';
    ctx.font = '14px monospace';
    ctx.textAlign = 'center';
    ctx.fillText('No depth on book.', width / 2, height / 2);
    setMetaText(metaElement, `Tick ${time}: no bid or ask depth.`);
    return;
  }

  const prices = bidBins.concat(askBins).map((bin) => bin.price);
  const qtys = bidBins.concat(askBins).map((bin) => bin.qty);
  const minPrice = Math.min(...prices);
  const maxPrice = Math.max(...prices);
  const maxQty = Math.max(1, ...qtys);
  const chartWidth = width - padding.left - padding.right;
  const chartHeight = height - padding.top - padding.bottom;
  const priceRange = Math.max(1, maxPrice - minPrice);
  const getX = (price: number) => padding.left + ((price - minPrice) / priceRange) * chartWidth;
  const getY = (qty: number) => padding.top + chartHeight - (qty / maxQty) * chartHeight;

  ctx.strokeStyle = '#e2e2e2';
  ctx.lineWidth = 1;
  for (let index = 0; index <= 4; index += 1) {
    const y = padding.top + (chartHeight * index) / 4;
    ctx.beginPath();
    ctx.moveTo(padding.left, y);
    ctx.lineTo(width - padding.right, y);
    ctx.stroke();
  }

  ctx.beginPath();
  ctx.moveTo(padding.left, padding.top);
  ctx.lineTo(padding.left, height - padding.bottom);
  ctx.lineTo(width - padding.right, height - padding.bottom);
  ctx.strokeStyle = '#999';
  ctx.stroke();

  drawDepthSeries(ctx, bidBins, getX, getY, '#0f766e', 'rgba(15, 118, 110, 0.16)');
  drawDepthSeries(ctx, askBins, getX, getY, '#b91c1c', 'rgba(185, 28, 28, 0.16)');

  ctx.fillStyle = '#333';
  ctx.font = '12px monospace';
  ctx.textAlign = 'left';
  ctx.fillText(`${maxQty}`, 8, padding.top + 4);
  ctx.fillText('0', 18, height - padding.bottom);
  ctx.fillText(`${minPrice}`, padding.left, height - 8);
  ctx.textAlign = 'right';
  ctx.fillText(`${maxPrice}`, width - padding.right, height - 8);

  ctx.textAlign = 'left';
  ctx.fillStyle = '#0f766e';
  ctx.fillText('Bid Depth', padding.left, 14);
  ctx.fillStyle = '#b91c1c';
  ctx.fillText('Ask Depth', padding.left + 84, 14);

  setMetaText(
    metaElement,
    `Tick ${time}: ${bidBins.length} bid bins, ${askBins.length} ask bins, top bid=${spread && !spread.bidsMissing ? spread.highestBid : 'NA'}, top ask=${spread && !spread.asksMissing ? spread.lowestAsk : 'NA'}`,
  );
}