/** \file shapemotion.c
 *  \brief This is a simple pong game
 */  
#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include <shape.h>
#include <abCircle.h>
#include "buzzer.h"

#define GREEN_LED BIT6

AbRect paddle1 = {abRectGetBounds, abRectCheck, {2, 10}}; /* 2x10 rectangle*/
AbRect paddle2 = {abRectGetBounds, abRectCheck, {2, 10}}; /* 2x10 rectangle*/
AbRect rect10 = {abRectGetBounds, abRectCheck, {10,10}}; /* 10x10 rectangle */

AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 - 1, screenHeight/2 - 20}
};

Layer paddleLayer1 = {
  (AbShape *)&paddle1,
  {12, screenHeight/2},
  {0,0}, {0,0},
  COLOR_RED,
  0
};

Layer paddleLayer2 = {
  (AbShape *)&paddle2,
  {screenWidth - 12, screenHeight/2},
  {0,0}, {0,0},
  COLOR_BLUE,
  &paddleLayer1,
};

Layer fieldLayer = {		/* playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  &paddleLayer2,
};

Layer layer0 = {		/**< Layer with a ball */
  (AbShape *)&circle8,
  {(screenWidth/2)+10, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_GREEN,
  &fieldLayer,
};


/** Moving Layer
 *  Linked list of layer references
 *  Velocity represents one iteration of change (direction & magnitude)
 */
typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

/* initial value of {0,0} will be overwritten */
MovLayer ml2 = { &paddleLayer2, {0,0}, 0 };
MovLayer ml1 = { &paddleLayer1, {0,0}, &ml2 };
MovLayer ml0 = { &layer0, {2,1}, &ml1 }; 

movLayerDraw(MovLayer *movLayers, Layer *layers)
{
  int row, col;
  MovLayer *movLayer;

  and_sr(~8);			/**< disable interrupts (GIE off) */
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);			/**< disable interrupts (GIE on) */


  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], 
		bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
      for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
	Vec2 pixelPos = {col, row};
	u_int color = bgColor;
	Layer *probeLayer;
	for (probeLayer = layers; probeLayer; 
	     probeLayer = probeLayer->next) { /* probe all layers, in order */
	  if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
	    color = probeLayer->color;
	    break; 
	  } /* if probe check */
	} // for checking all layers at col, row
	lcd_writeColor(color); 
      } // for col
    } // for row
  } // for moving layer being updated
}	  



//Region fence = {{10,30}, {SHORT_EDGE_PIXELS-10, LONG_EDGE_PIXELS-10}}; /**< Create a fence region */

/** Advances a moving shape within a fence
 *  Checks for collisions with a paddle and if so then bounce off
 *
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */
void mlAdvance(MovLayer *ml, Region *fence)
{
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  for (; ml; ml = ml->next) {
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    for (axis = 0; axis < 2; axis ++) {
      if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) ||
	  (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) {
	int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	newPos.axes[axis] += (2*velocity);
      }	/**< if outside of fence */
      if(isCollision(&ml0, &ml1))
	{
      	  int vel = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
      	  newPos.axes[axis] += (2*vel);
      	}
      if(isCollision(&ml0, &ml2))
	{
	  int vel = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
      	  newPos.axes[axis] += (2*vel);
      	}
    } /**< for axis */
    ml->layer->posNext = newPos;
  } /**< for ml */
}

/* 
 * This was done with collaboration with Luis Cuellar. He helped me with
 * the logic of a shape colliding with another.
 *
 * Much of the logic was taken from mlAdvanced. Detect if a ball collided
 * with a paddle layer and if so then return 1.
*/
int isCollision(const MovLayer *ml, const MovLayer *paddle)
{
  Region ball, p, inter; //ball, paddle, shape intersect

  abShapeGetBounds(ml->layer->abShape, &(ml->layer->pos), &ball);
  abShapeGetBounds(paddle->layer->abShape, &(paddle->layer->pos), &p);
  shapeIntersect(&inter, &ball, &p);  
  
  int x1 = inter.topLeft.axes[0];
  int x2 = inter.botRight.axes[0];
  int y1 = inter.topLeft.axes[1];
  int y2 = inter.botRight.axes[1];
  
  if(x1 >= x2 || y1 >= y2)
    {
      return 0;
    }
  return 1;
}

/* Logic to detect intersections between the paddle and the ball. */
void shapeIntersect(Region *sIntersect, const Region *r1, const Region *r2)
{
  vec2Max(&sIntersect->topLeft, &r1->topLeft, &r2->topLeft);
  vec2Min(&sIntersect->botRight, &r1->botRight, &r2->botRight);
}

/* Uses isCollision to detect when a ball hits a paddle and if it
 * does then simply play a sound. Different paddles play different
 * sound tone.
 */
void check_play_sound()
{
  if(isCollision(&ml0, &ml1))
    {
      buzzer_play(2000);
    }
  if(isCollision(&ml0, &ml2))
    {
      buzzer_play(2500);
    }
}

int scorep1 = 0; //score counter for player 1
int scorep2 = 0; //Score counter for player 2

/* Detects when the ball touches either the left or right 
 * side of the playing field and increments the corresponding
 * player's score. This is a modified version of the mlAdvance function.
 */
void check_ifScore(MovLayer *ml, Region *fence)
{
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  for (; ml; ml = ml->next) {
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    if(shapeBoundary.topLeft.axes[0] < fence->topLeft.axes[0]) {
      scorep2 += 1;
    }
    if(shapeBoundary.botRight.axes[0] > fence->botRight.axes[0]) {
      scorep1 += 1;
    }
  } /**< for ml */
}

void playerWin(char str[])
{
  /* Implement a winning message while pausing
   * the game then afterwards reset the game
   */
}

u_int bgColor = COLOR_VIOLET;     /**< The background color */
int redrawScreen = 1;           /**< Boolean for whether screen needs to be redrawn */

Region fieldFence;		/**< fence around playing field  */

/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */
void main()
{
  P1DIR |= GREEN_LED;		/**< Green led on when CPU on */		
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  shapeInit();
  p2sw_init(15);

  shapeInit();

  layerInit(&layer0);
  layerDraw(&layer0);

  layerGetBounds(&fieldLayer, &fieldFence);

  enableWDTInterrupts();      /**< enable periodic interrupt */
  buzzer_init();
  or_sr(0x8);	              /**< GIE (enable interrupts) */

  drawString5x7(screenWidth/2 - 15, 0, "score:", COLOR_BLACK, COLOR_VIOLET);
  
  for(;;) {
    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);	      /**< CPU OFF */
    }
    //Set score array to store score variables for players
    char scoreArray[2];
    scoreArray[0] = '0' + scorep1; //Score for player 1
    scoreArray[1] = '|';           //Simple barrier to seperate scores
    scoreArray[2] = '0' + scorep2; //Score for player 2
    drawString5x7(screenWidth/2 - 5, 10, scoreArray, COLOR_BLACK, COLOR_VIOLET);
    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
    movLayerDraw(&ml0, &layer0);
  }
}

/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  if (count == 15) {
    mlAdvance(&ml0, &fieldFence);
    check_ifScore(&ml0, &fieldFence);
    ml1.velocity.axes[1] = 0;
    ml2.velocity.axes[1] = 0;
    buzzer_play(0);
    check_play_sound();

    if(scorep1 == 10 || scorep2 == 10)
      {
	//playerWin("Player 1");
	scorep1 = 0;
	scorep2 = 0;
      }
    /*
    if(scorep2 == 10)
      {
	//playerWin("Player 2");
      } 
    */
    /* Logic for button press. */
    //Make paddle1 go up
    if (!(p2sw_read() & BIT0))
      {
	ml1.velocity.axes[1] = -1; 
      }
    //Make paddle1 go down
    if(!(p2sw_read() & BIT1))
      {
	ml1.velocity.axes[1] = 1;
      }
    //Make paddle2 go up
    if(!(p2sw_read() & BIT2))
      {
	ml2.velocity.axes[1] = -1;
      }
    //Make paddle2 go down
    if(!(p2sw_read() & BIT3))
      {
	ml2.velocity.axes[1] = 1;
      }
    redrawScreen = 1;
    count = 0;
  } 
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}
